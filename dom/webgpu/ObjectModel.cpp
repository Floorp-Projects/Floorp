/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ObjectModel.h"

#include "Adapter.h"
#include "ShaderModule.h"
#include "CompilationInfo.h"
#include "Device.h"
#include "CommandEncoder.h"
#include "Instance.h"
#include "Texture.h"
#include "nsIGlobalObject.h"

namespace mozilla::webgpu {

template <typename T>
ChildOf<T>::ChildOf(T* const parent) : mParent(parent) {}

template <typename T>
ChildOf<T>::~ChildOf() = default;

template <typename T>
nsIGlobalObject* ChildOf<T>::GetParentObject() const {
  return mParent->GetParentObject();
}

void ObjectBase::GetLabel(nsAString& aValue) const { aValue = mLabel; }
void ObjectBase::SetLabel(const nsAString& aLabel) { mLabel = aLabel; }

template class ChildOf<Adapter>;
template class ChildOf<ShaderModule>;
template class ChildOf<CompilationInfo>;
template class ChildOf<CommandEncoder>;
template class ChildOf<Device>;
template class ChildOf<Instance>;
template class ChildOf<Texture>;

}  // namespace mozilla::webgpu
