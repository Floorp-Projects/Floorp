/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ANDROID_IMEDIARESOURCEMANAGERSERVICE_H
#define ANDROID_IMEDIARESOURCEMANAGERSERVICE_H

#include <utils/RefBase.h>
#include <binder/IInterface.h>

#include "IMediaResourceManagerClient.h"


namespace android {

// ----------------------------------------------------------------------------

class IMediaResourceManagerService : public IInterface
{
public:
    DECLARE_META_INTERFACE(MediaResourceManagerService);

    // Request a media resource for IMediaResourceManagerClient.
    virtual void requestMediaResource(const sp<IMediaResourceManagerClient>& client, int resourceType) = 0;
    // Cancel a media resource request and a resource allocated to IMediaResourceManagerClient.
    virtual status_t cancelClient(const sp<IMediaResourceManagerClient>& client) = 0;
};


// ----------------------------------------------------------------------------

class BnMediaResourceManagerService : public BnInterface<IMediaResourceManagerService>
{
public:
    virtual status_t    onTransact( uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);
};

// ----------------------------------------------------------------------------

}; // namespace android

#endif // ANDROID_IMEDIARESOURCEMANAGERSERVICE_H
