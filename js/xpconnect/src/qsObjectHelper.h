/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef qsObjectHelper_h
#define qsObjectHelper_h

#include "xpcObjectHelper.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsINode.h"
#include "nsWrapperCache.h"
#include "mozilla/TypeTraits.h"

template <typename T>
struct qsIsNode
{
    static const bool value =
        mozilla::IsBaseOf<nsINode, T>::value ||
        mozilla::IsBaseOf<nsIDOMNode, T>::value;
};

class qsObjectHelper : public xpcObjectHelper
{
public:
    template <class T>
    inline
    qsObjectHelper(T *aObject, nsWrapperCache *aCache)
        : xpcObjectHelper(ToSupports(aObject), ToCanonicalSupports(aObject),
                          aCache, qsIsNode<T>::value)
    {}

    template <class T>
    inline
    qsObjectHelper(nsCOMPtr<T>& aObject, nsWrapperCache *aCache)
        : xpcObjectHelper(ToSupports(aObject.get()),
                          ToCanonicalSupports(aObject.get()), aCache,
                          qsIsNode<T>::value)
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
                          ToCanonicalSupports(aObject.get()), aCache,
                          qsIsNode<T>::value)
    {
        if (mCanonical) {
            // Transfer the strong reference.
            mCanonicalStrong = dont_AddRef(mCanonical);
            aObject.forget();
        }
    }
};

#endif
