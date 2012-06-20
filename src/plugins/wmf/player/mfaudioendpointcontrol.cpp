/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the Qt Mobility Components.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "QtCore/qdebug.h"
#include "mfaudioendpointcontrol.h"

MFAudioEndpointControl::MFAudioEndpointControl(QObject *parent)
    : QAudioEndpointSelectorControl(parent)
    , m_currentActivate(0)
{
    updateEndpoints();
    setActiveEndpoint(m_defaultEndpoint);
}

MFAudioEndpointControl::~MFAudioEndpointControl()
{
    foreach (LPWSTR wstrID, m_devices)
         CoTaskMemFree(wstrID);

    if (m_currentActivate)
        m_currentActivate->Release();
}

QList<QString> MFAudioEndpointControl::availableEndpoints() const
{
    return m_devices.keys();
}

QString MFAudioEndpointControl::endpointDescription(const QString &name) const
{
    return name.section(QLatin1Char('\\'), -1);
}

QString MFAudioEndpointControl::defaultEndpoint() const
{
    return m_defaultEndpoint;
}

QString MFAudioEndpointControl::activeEndpoint() const
{
    return m_activeEndpoint;
}

void MFAudioEndpointControl::setActiveEndpoint(const QString &name)
{
    if (m_activeEndpoint == name)
        return;
    QMap<QString, LPWSTR>::iterator it = m_devices.find(name);
    if (it == m_devices.end())
        return;

    LPWSTR wstrID = *it;
    IMFActivate *activate = NULL;
    HRESULT hr = MFCreateAudioRendererActivate(&activate);
    if (FAILED(hr)) {
        qWarning() << "Failed to create audio renderer activate";
        return;
    }

    if (wstrID) {
        hr = activate->SetString(MF_AUDIO_RENDERER_ATTRIBUTE_ENDPOINT_ID, wstrID);
    } else {
        //This is the default one that has been inserted in updateEndpoints(),
        //so give the activate a hint that we want to use the device for multimedia playback
        //then the media foundation will choose an appropriate one.

        //from MSDN:
        //The ERole enumeration defines constants that indicate the role that the system has assigned to an audio endpoint device.
        //eMultimedia: Music, movies, narration, and live music recording.
        hr = activate->SetUINT32(MF_AUDIO_RENDERER_ATTRIBUTE_ENDPOINT_ROLE, eMultimedia);
    }

    if (FAILED(hr)) {
        qWarning() << "Failed to set attribute for audio device" << name;
        return;
    }

    if (m_currentActivate)
        m_currentActivate->Release();
    m_currentActivate = activate;
    m_activeEndpoint = name;
}

IMFActivate*  MFAudioEndpointControl::currentActivate() const
{
    return m_currentActivate;
}

void MFAudioEndpointControl::updateEndpoints()
{
    m_defaultEndpoint = QString::fromLatin1("Default");
    m_devices.insert(m_defaultEndpoint, NULL);

    IMMDeviceEnumerator *pEnum = NULL;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                          NULL, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator),
                         (void**)&pEnum);
    if (SUCCEEDED(hr)) {
        IMMDeviceCollection *pDevices = NULL;
        hr = pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices);
        if (SUCCEEDED(hr)) {
            UINT count;
            hr = pDevices->GetCount(&count);
            if (SUCCEEDED(hr)) {
                for (UINT i = 0; i < count; ++i) {
                    IMMDevice *pDevice = NULL;
                    hr = pDevices->Item(i, &pDevice);
                    if (SUCCEEDED(hr)) {
                        LPWSTR wstrID = NULL;
                        hr = pDevice->GetId(&wstrID);
                        if (SUCCEEDED(hr)) {
                            QString deviceId = QString::fromWCharArray(wstrID);
                            m_devices.insert(deviceId, wstrID);
                        }
                        pDevice->Release();
                    }
                }
            }
            pDevices->Release();
        }
        pEnum->Release();
    }
}
