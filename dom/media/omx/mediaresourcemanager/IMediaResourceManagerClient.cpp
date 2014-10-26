/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

//#define LOG_NDEBUG 0
#define LOG_TAG "IMediaResourceManagerClient"
#include <utils/Log.h>

#include <stdint.h>
#include <sys/types.h>

#include <binder/Parcel.h>

#include "IMediaResourceManagerClient.h"

namespace android {

enum {
    STATUS_CHANGED = IBinder::FIRST_CALL_TRANSACTION
};

class BpMediaResourceManagerClient : public BpInterface<IMediaResourceManagerClient>
{
public:
    BpMediaResourceManagerClient(const sp<IBinder>& impl)
        : BpInterface<IMediaResourceManagerClient>(impl)
    {
    }

    void statusChanged(int event)
    {
        Parcel data, reply;
        data.writeInterfaceToken(IMediaResourceManagerClient::getInterfaceDescriptor());
        data.writeInt32(event);
        remote()->transact(STATUS_CHANGED, data, &reply, IBinder::FLAG_ONEWAY);
    }
};

IMPLEMENT_META_INTERFACE(MediaResourceManagerClient, "android.media.IMediaResourceManagerClient");

// ----------------------------------------------------------------------

status_t BnMediaResourceManagerClient::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    switch(code) {
    case STATUS_CHANGED: {
            CHECK_INTERFACE(IMediaResourceManagerClient, data, reply);
            int event = data.readInt32();
            statusChanged(event);
            return NO_ERROR;
        } break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

// ----------------------------------------------------------------------------

}; // namespace android
