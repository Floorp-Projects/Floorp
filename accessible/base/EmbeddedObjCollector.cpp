/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbeddedObjCollector.h"

#include "LocalAccessible.h"

using namespace mozilla::a11y;

uint32_t EmbeddedObjCollector::Count() {
  EnsureNGetIndex(nullptr);
  return mObjects.Length();
}

LocalAccessible* EmbeddedObjCollector::GetAccessibleAt(uint32_t aIndex) {
  LocalAccessible* accessible = mObjects.SafeElementAt(aIndex, nullptr);
  if (accessible) return accessible;

  return EnsureNGetObject(aIndex);
}

LocalAccessible* EmbeddedObjCollector::EnsureNGetObject(uint32_t aIndex) {
  uint32_t childCount = mRoot->ChildCount();
  while (mRootChildIdx < childCount) {
    LocalAccessible* child = mRoot->LocalChildAt(mRootChildIdx++);
    if (child->IsText()) continue;

    AppendObject(child);
    if (mObjects.Length() - 1 == aIndex) return mObjects[aIndex];
  }

  return nullptr;
}

int32_t EmbeddedObjCollector::EnsureNGetIndex(LocalAccessible* aAccessible) {
  uint32_t childCount = mRoot->ChildCount();
  while (mRootChildIdx < childCount) {
    LocalAccessible* child = mRoot->LocalChildAt(mRootChildIdx++);
    if (child->IsText()) continue;

    AppendObject(child);
    if (child == aAccessible) return mObjects.Length() - 1;
  }

  return -1;
}

int32_t EmbeddedObjCollector::GetIndexAt(LocalAccessible* aAccessible) {
  if (aAccessible->mParent != mRoot) return -1;

  if (aAccessible->mIndexOfEmbeddedChild != -1) {
    return aAccessible->mIndexOfEmbeddedChild;
  }

  return !aAccessible->IsText() ? EnsureNGetIndex(aAccessible) : -1;
}

void EmbeddedObjCollector::AppendObject(LocalAccessible* aAccessible) {
  aAccessible->mIndexOfEmbeddedChild = mObjects.Length();
  mObjects.AppendElement(aAccessible);
}
