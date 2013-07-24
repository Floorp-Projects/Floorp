/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef qsObjectHelper_h
#define qsObjectHelper_h

#include "xpcObjectHelper.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "mozilla/TypeTraits.h"

class qsObjectHelper : public xpcObjectHelper
{
public:
    template <class T>
    inline
    qsObjectHelper(T *aObject, nsWrapperCache *aCache)
        : xpcObjectHelper(ToSupports(aObject), ToCanonicalSupports(aObject),
                          aCache)
    {}

    template <class T>
    inline
    qsObjectHelper(nsCOMPtr<T>& aObject, nsWrapperCache *aCache)
        : xpcObjectHelper(ToSupports(aObject.get()),
                          ToCanonicalSupports(aObject.get()), aCache)
    {
        if (mCanonical) {
            // Transfer the strong reference.
            mCanonicalStrong = dont_AddRef(mCanonical);
            aObject.forget();
        }
    }

    template <class T>
    inline
    qsObjectHelper(nsRefPtr<T>& aObject, nsWrapperCache *aCache)
        : xpcObjectHelper(ToSupports(aObject.get()),
                          ToCanonicalSupports(aObject.get()), aCache)
    {
        if (mCanonical) {
            // Transfer the strong reference.
            mCanonicalStrong = dont_AddRef(mCanonical);
            aObject.forget();
        }
    }
};

#endif
