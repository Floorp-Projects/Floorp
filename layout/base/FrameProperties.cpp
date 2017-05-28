/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameProperties.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/ServoStyleSet.h"
#include "nsThreadUtils.h"

namespace mozilla {

void
FrameProperties::SetInternal(UntypedDescriptor aProperty, void* aValue,
                             const nsIFrame* aFrame)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aProperty, "Null property?");

  nsTArray<PropertyValue>::index_type index =
    mProperties.IndexOf(aProperty, 0, PropertyComparator());
  if (index != nsTArray<PropertyValue>::NoIndex) {
    PropertyValue* pv = &mProperties.ElementAt(index);
    pv->DestroyValueFor(aFrame);
    pv->mValue = aValue;
    return;
  }

  mProperties.AppendElement(PropertyValue(aProperty, aValue));
#ifdef DEBUG
  mMaxLength = std::max<uint32_t>(mMaxLength, mProperties.Length());
#endif
}

void*
FrameProperties::RemoveInternal(UntypedDescriptor aProperty, bool* aFoundResult)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aProperty, "Null property?");

  nsTArray<PropertyValue>::index_type index =
    mProperties.IndexOf(aProperty, 0, PropertyComparator());
  if (index == nsTArray<PropertyValue>::NoIndex) {
    if (aFoundResult) {
      *aFoundResult = false;
    }
    return nullptr;
  }

  if (aFoundResult) {
    *aFoundResult = true;
  }

  void* result = mProperties.ElementAt(index).mValue;
  mProperties.RemoveElementAt(index);

  return result;
}

void
FrameProperties::DeleteInternal(UntypedDescriptor aProperty,
                                const nsIFrame* aFrame)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aProperty, "Null property?");

  bool found;
  void* v = RemoveInternal(aProperty, &found);
  if (found) {
    PropertyValue pv(aProperty, v);
    pv.DestroyValueFor(aFrame);
  }
}

void
FrameProperties::DeleteAll(const nsIFrame* aFrame)
{
  for (uint32_t i = 0; i < mProperties.Length(); ++i) {
    mProperties.ElementAt(i).DestroyValueFor(aFrame);
  }

  mProperties.Clear();
}

size_t
FrameProperties::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return mProperties.Length() * sizeof(PropertyValue);
}

} // namespace mozilla
