/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcObjectHelper_h
#define xpcObjectHelper_h

// Including 'windows.h' will #define GetClassInfo to something else.
#ifdef XP_WIN
#ifdef GetClassInfo
#undef GetClassInfo
#endif
#endif

#include "mozilla/Attributes.h"
#include "mozilla/StandardInteger.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIClassInfo.h"
#include "nsINode.h"
#include "nsISupports.h"
#include "nsIXPCScriptable.h"
#include "nsWrapperCache.h"

class xpcObjectHelper
{
public:
    xpcObjectHelper(nsISupports *aObject, nsWrapperCache *aCache = NULL)
      : mCanonical(NULL)
      , mObject(aObject)
      , mCache(aCache)
      , mIsNode(false)
    {
        if (!mCache) {
            if (aObject)
                CallQueryInterface(aObject, &mCache);
            else
                mCache = NULL;
        }
    }

    nsISupports *Object()
    {
        return mObject;
    }

    nsISupports *GetCanonical()
    {
        if (!mCanonical) {
            mCanonicalStrong = do_QueryInterface(mObject);
            mCanonical = mCanonicalStrong;
        }
        return mCanonical;
    }

    already_AddRefed<nsISupports> forgetCanonical()
    {
        NS_ASSERTION(mCanonical, "Huh, no canonical to forget?");

        if (!mCanonicalStrong)
            mCanonicalStrong = mCanonical;
        mCanonical = NULL;
        return mCanonicalStrong.forget();
    }

    nsIClassInfo *GetClassInfo()
    {
        if (mXPCClassInfo)
          return mXPCClassInfo;
        if (!mClassInfo)
            mClassInfo = do_QueryInterface(mObject);
        return mClassInfo;
    }
    nsXPCClassInfo *GetXPCClassInfo()
    {
        if (!mXPCClassInfo) {
            if (mIsNode)
                mXPCClassInfo = static_cast<nsINode*>(GetCanonical())->GetClassInfo();
            else
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
            sinfo = do_QueryInterface(GetCanonical());

        // We should have something by now.
        MOZ_ASSERT(sinfo);

        // Grab the flags. This should not fail.
        PRUint32 flags;
        mozilla::DebugOnly<nsresult> rv = sinfo->GetScriptableFlags(&flags);
        MOZ_ASSERT(NS_SUCCEEDED(rv));

        return flags;
    }

    nsWrapperCache *GetWrapperCache()
    {
        return mCache;
    }

protected:
    xpcObjectHelper(nsISupports *aObject, nsISupports *aCanonical,
                    nsWrapperCache *aCache, bool aIsNode)
      : mCanonical(aCanonical)
      , mObject(aObject)
      , mCache(aCache)
      , mIsNode(aIsNode)
    {
        if (!mCache && aObject)
            CallQueryInterface(aObject, &mCache);
    }

    nsCOMPtr<nsISupports>    mCanonicalStrong;
    nsISupports*             mCanonical;

private:
    xpcObjectHelper(xpcObjectHelper& aOther) MOZ_DELETE;

    nsISupports*             mObject;
    nsWrapperCache*          mCache;
    nsCOMPtr<nsIClassInfo>   mClassInfo;
    nsRefPtr<nsXPCClassInfo> mXPCClassInfo;
    bool                     mIsNode;
};

#endif
