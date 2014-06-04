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

    // Enumeration for the resource types
    enum ResourceType {
      HW_VIDEO_DECODER = 0,
      HW_AUDIO_DECODER,  // Not supported currently.
      HW_VIDEO_ENCODER,
      HW_CAMERA,          // Not supported currently.
      NUM_OF_RESOURCE_TYPES,
      INVALID_RESOURCE_TYPE = -1
    };

    enum ErrorCode {
        RESOURCE_NOT_AVAILABLE = -EAGAIN
    };

    // Request a media resource for IMediaResourceManagerClient.
    // client is the binder that service will notify (through
    // IMediaResourceManagerClient::statusChanged()) when request status changed.
    // resourceType is type of resource that client would like to request.
    // willWait indicates that, when the resource is not currently available
    // (i.e., already in use by another client), if the client wants to wait. If
    // true, client will be put into a (FIFO) waiting list and be notified when
    // resource is available.
    // For unsupported types, this function returns BAD_TYPE. For supported
    // types, it always returns OK when willWait is true; otherwise it will
    // return true when resouce is available, or RESOURCE_NOT_AVAILABLE when
    // resouce is in use.
    virtual status_t requestMediaResource(const sp<IMediaResourceManagerClient>& client, int resourceType, bool willWait) = 0;
    // Cancel a media resource request and a resource allocated to IMediaResourceManagerClient.
    // Client must call this function after it's done with the media resource requested.
    virtual status_t cancelClient(const sp<IMediaResourceManagerClient>& client, int resourceType) = 0;
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
