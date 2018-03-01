/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcObjectHelper_h
#define xpcObjectHelper_h

// Including 'windows.h' will #define GetClassInfo to something else.
#ifdef XP_WIN
#ifdef GetClassInfo
#undef GetClassInfo
#endif
#endif

#include "mozilla/Attributes.h"
#include <stdint.h>
#include "nsCOMPtr.h"
#include "nsIClassInfo.h"
#include "nsISupports.h"
#include "nsIXPCScriptable.h"
#include "nsWrapperCache.h"

class xpcObjectHelper
{
public:
    explicit xpcObjectHelper(nsISupports* aObject, nsWrapperCache* aCache = nullptr)
      : mObject(aObject)
      , mCache(aCache)
    {
        if (!mCache && aObject) {
            CallQueryInterface(aObject, &mCache);
        }
    }

    nsISupports* Object()
    {
        return mObject;
    }

    nsIClassInfo* GetClassInfo()
    {
        if (mXPCClassInfo)
          return mXPCClassInfo;
        if (!mClassInfo)
            mClassInfo = do_QueryInterface(mObject);
        return mClassInfo;
    }
    nsXPCClassInfo* GetXPCClassInfo()
    {
        if (!mXPCClassInfo) {
            CallQueryInterface(mObject, getter_AddRefs(mXPCClassInfo));
        }
        return mXPCClassInfo;
    }

    already_AddRefed<nsXPCClassInfo> forgetXPCClassInfo()
    {
        GetXPCClassInfo();

        return mXPCClassInfo.forget();
    }

    // We assert that we can reach an nsIXPCScriptable somehow.
    uint32_t GetScriptableFlags()
    {
        // Try getting an nsXPCClassInfo - this handles DOM scriptable helpers.
        nsCOMPtr<nsIXPCScriptable> sinfo = GetXPCClassInfo();

        // If that didn't work, try just QI-ing. This handles BackstagePass.
        if (!sinfo)
            sinfo = do_QueryInterface(mObject);

        // We should have something by now.
        MOZ_ASSERT(sinfo);

        // Grab the flags.
        return sinfo->GetScriptableFlags();
    }

    nsWrapperCache* GetWrapperCache()
    {
        return mCache;
    }

private:
    xpcObjectHelper(xpcObjectHelper& aOther) = delete;

    nsISupports* MOZ_UNSAFE_REF("xpcObjectHelper has been specifically optimized "
                                "to avoid unnecessary AddRefs and Releases. "
                                "(see bug 565742)") mObject;
    nsWrapperCache*          mCache;
    nsCOMPtr<nsIClassInfo>   mClassInfo;
    RefPtr<nsXPCClassInfo> mXPCClassInfo;
};

#endif
