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

//#define LOG_NDEBUG 0
#define LOG_TAG "IMediaResourceManagerDeathNotifier"
#include <utils/Log.h>

#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>

#include "IMediaResourceManagerDeathNotifier.h"

namespace android {

// client singleton for binder interface to services
Mutex IMediaResourceManagerDeathNotifier::sServiceLock;
sp<IMediaResourceManagerService> IMediaResourceManagerDeathNotifier::sMediaResourceManagerService;
sp<IMediaResourceManagerDeathNotifier::DeathNotifier> IMediaResourceManagerDeathNotifier::sDeathNotifier;
SortedVector< wp<IMediaResourceManagerDeathNotifier> > IMediaResourceManagerDeathNotifier::sObitRecipients;

// establish binder interface to MediaResourceManagerService
/*static*/const sp<IMediaResourceManagerService>&
IMediaResourceManagerDeathNotifier::getMediaResourceManagerService()
{
    LOGV("getMediaResourceManagerService");
    Mutex::Autolock _l(sServiceLock);
    if (sMediaResourceManagerService.get() == 0) {
        sp<IServiceManager> sm = defaultServiceManager();
        sp<IBinder> binder;
        do {
            binder = sm->getService(String16("media.resource_manager"));
            if (binder != 0) {
                break;
             }
             LOGW("Media resource manager service not published, waiting...");
             usleep(500000); // 0.5 s
        } while(true);

        if (sDeathNotifier == NULL) {
        sDeathNotifier = new DeathNotifier();
    }
    binder->linkToDeath(sDeathNotifier);
    sMediaResourceManagerService = interface_cast<IMediaResourceManagerService>(binder);
    }
    LOGE_IF(sMediaResourceManagerService == 0, "no media player service!?");
    return sMediaResourceManagerService;
}

/*static*/ void
IMediaResourceManagerDeathNotifier::addObitRecipient(const wp<IMediaResourceManagerDeathNotifier>& recipient)
{
    LOGV("addObitRecipient");
    Mutex::Autolock _l(sServiceLock);
    sObitRecipients.add(recipient);
}

/*static*/ void
IMediaResourceManagerDeathNotifier::removeObitRecipient(const wp<IMediaResourceManagerDeathNotifier>& recipient)
{
    LOGV("removeObitRecipient");
    Mutex::Autolock _l(sServiceLock);
    sObitRecipients.remove(recipient);
}

void
IMediaResourceManagerDeathNotifier::DeathNotifier::binderDied(const wp<IBinder>& who)
{
    LOGW("media server died");

    // Need to do this with the lock held
    SortedVector< wp<IMediaResourceManagerDeathNotifier> > list;
    {
        Mutex::Autolock _l(sServiceLock);
        sMediaResourceManagerService.clear();
        list = sObitRecipients;
    }

    // Notify application when media server dies.
    // Don't hold the static lock during callback in case app
    // makes a call that needs the lock.
    size_t count = list.size();
    for (size_t iter = 0; iter < count; ++iter) {
        sp<IMediaResourceManagerDeathNotifier> notifier = list[iter].promote();
        if (notifier != 0) {
            notifier->died();
        }
    }
}

IMediaResourceManagerDeathNotifier::DeathNotifier::~DeathNotifier()
{
    LOGV("DeathNotifier::~DeathNotifier");
    Mutex::Autolock _l(sServiceLock);
    sObitRecipients.clear();
    if (sMediaResourceManagerService != 0) {
        sMediaResourceManagerService->asBinder()->unlinkToDeath(this);
    }
}

}; // namespace android
