/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_ARRAY_VIEW_H_
#define MOZILLA_GFX_ARRAY_VIEW_H_

#include "nsTArray.h"

namespace mozilla {
namespace gfx {

template<typename T>
class ArrayView
{
    public:
        MOZ_IMPLICIT ArrayView(const nsTArray<T>& aData) :
            mData(aData.Elements()), mLength(aData.Length())
        {
        }
        ArrayView(const T* aData, const size_t aLength) :
            mData(aData), mLength(aLength)
        {
        }
        const T& operator[](const size_t aIdx) const
        {
            return mData[aIdx];
        }
        size_t Length() const
        {
            return mLength;
        }
        const T* Data() const
        {
            return mData;
        }
    private:
        const T* mData;
        const size_t mLength;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_ARRAY_VIEW_H_ */
