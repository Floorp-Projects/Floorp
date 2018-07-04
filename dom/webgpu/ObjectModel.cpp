/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ObjectModel.h"

#include "Adapter.h"
#include "Device.h"
#include "Instance.h"

namespace mozilla {
namespace webgpu {

template<typename T>
ChildOf<T>::ChildOf(T* const parent)
    : mParent(parent)
{ }

template<typename T>
ChildOf<T>::~ChildOf() = default;

template<typename T>
nsIGlobalObject*
ChildOf<T>::GetParentObject() const
{
    return mParent->GetParentObject();
}

template class ChildOf<Adapter>;
template class ChildOf<Device>;
template class ChildOf<Instance>;

} // namespace webgpu
} // namespace mozilla
