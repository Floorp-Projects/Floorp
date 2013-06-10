/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//#define LOG_NDEBUG 0
#define LOG_TAG "IMediaResourceManagerService"

#include <stdint.h>
#include <sys/types.h>

#include <binder/Parcel.h>
#include <utils/Log.h>

#include "IMediaResourceManagerService.h"

namespace android {

/**
 * Function ID used between BpMediaResourceManagerService and 
 *  BnMediaResourceManagerService by using Binder ipc.
 */
enum {
    REQUEST_MEDIA_RESOURCE = IBinder::FIRST_CALL_TRANSACTION,
    DEREGISTER_CLIENT
};

class BpMediaResourceManagerService : public BpInterface<IMediaResourceManagerService>
{
public:
    BpMediaResourceManagerService(const sp<IBinder>& impl)
        : BpInterface<IMediaResourceManagerService>(impl)
    {
    }

    virtual void requestMediaResource(const sp<IMediaResourceManagerClient>& client, int resourceType)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IMediaResourceManagerService::getInterfaceDescriptor());
        data.writeStrongBinder(client->asBinder());
        data.writeInt32(resourceType);
        remote()->transact(REQUEST_MEDIA_RESOURCE, data, &reply);
    }

    virtual status_t cancelClient(const sp<IMediaResourceManagerClient>& client)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IMediaResourceManagerService::getInterfaceDescriptor());
        data.writeStrongBinder(client->asBinder());
        remote()->transact(DEREGISTER_CLIENT, data, &reply);
        return reply.readInt32();
    }
};

IMPLEMENT_META_INTERFACE(MediaResourceManagerService, "android.media.IMediaResourceManagerService");

// ----------------------------------------------------------------------

status_t BnMediaResourceManagerService::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {

        case REQUEST_MEDIA_RESOURCE: {
            CHECK_INTERFACE(IMediaResourceManagerService, data, reply);
            sp<IMediaResourceManagerClient> client = interface_cast<IMediaResourceManagerClient>(data.readStrongBinder());
            int resourceType = data.readInt32();
            requestMediaResource(client, resourceType);
            return NO_ERROR;
        } break;
        case DEREGISTER_CLIENT: {
            CHECK_INTERFACE(IMediaResourceManagerService, data, reply);
            sp<IMediaResourceManagerClient> client = interface_cast<IMediaResourceManagerClient>(data.readStrongBinder());
            cancelClient(client);
            reply->writeInt32(NO_ERROR);
            return NO_ERROR;
        } break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

// ----------------------------------------------------------------------------

}; // namespace android
