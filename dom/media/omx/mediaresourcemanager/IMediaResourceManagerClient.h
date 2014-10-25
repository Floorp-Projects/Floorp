/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ANDROID_IMEDIARESOURCEMANAGERCLIENT_H
#define ANDROID_IMEDIARESOURCEMANAGERCLIENT_H

#include <utils/RefBase.h>
#include <binder/IInterface.h>

namespace android {

// ----------------------------------------------------------------------------

class IMediaResourceManagerClient : public IInterface
{
public:
    DECLARE_META_INTERFACE(MediaResourceManagerClient);

    // Notifies a change of media resource request status.
    virtual void statusChanged(int event) = 0;

};


// ----------------------------------------------------------------------------

class BnMediaResourceManagerClient : public BnInterface<IMediaResourceManagerClient>
{
public:
    virtual status_t    onTransact( uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);
};

// ----------------------------------------------------------------------------

}; // namespace android

#endif // ANDROID_IMEDIARESOURCEMANAGERCLIENT_H
