/*
** Copyright 2010, The Android Open Source Project
** Copyright 2013, Mozilla Foundation
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef ANDROID_IMEDIARESOURCEMANAGERDEATHNOTIFIER_H
#define ANDROID_IMEDIARESOURCEMANAGERDEATHNOTIFIER_H

#include <utils/threads.h>
#include <utils/SortedVector.h>

#include "IMediaResourceManagerService.h"

namespace android {

/**
 * Handle MediaResourceManagerService's death notification.
 * Made from android's IMediaDeathNotifier class.
 */
class IMediaResourceManagerDeathNotifier: virtual public RefBase
{
public:
    IMediaResourceManagerDeathNotifier() { addObitRecipient(this); }
    virtual ~IMediaResourceManagerDeathNotifier() { removeObitRecipient(this); }

    virtual void died() = 0;
    static const sp<IMediaResourceManagerService>& getMediaResourceManagerService();

private:
    IMediaResourceManagerDeathNotifier &operator=(const IMediaResourceManagerDeathNotifier &);
    IMediaResourceManagerDeathNotifier(const IMediaResourceManagerDeathNotifier &);

    static void addObitRecipient(const wp<IMediaResourceManagerDeathNotifier>& recipient);
    static void removeObitRecipient(const wp<IMediaResourceManagerDeathNotifier>& recipient);

    class DeathNotifier: public IBinder::DeathRecipient
    {
    public:
                DeathNotifier() {}
        virtual ~DeathNotifier();

        virtual void binderDied(const wp<IBinder>& who);
    };

    friend class DeathNotifier;

    static  Mutex                                   sServiceLock;
    static  sp<IMediaResourceManagerService>        sMediaResourceManagerService;
    static  sp<DeathNotifier>                       sDeathNotifier;
    static  SortedVector< wp<IMediaResourceManagerDeathNotifier> > sObitRecipients;
};

}; // namespace android

#endif // ANDROID_IMEDIARESOURCEMANAGERDEATHNOTIFIER_H
