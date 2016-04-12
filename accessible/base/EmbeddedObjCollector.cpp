/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmbeddedObjCollector.h"

#include "Accessible.h"

using namespace mozilla::a11y;

uint32_t
EmbeddedObjCollector::Count()
{
  EnsureNGetIndex(nullptr);
  return mObjects.Length();
}

Accessible*
EmbeddedObjCollector::GetAccessibleAt(uint32_t aIndex)
{
  Accessible* accessible = mObjects.SafeElementAt(aIndex, nullptr);
  if (accessible)
    return accessible;

  return EnsureNGetObject(aIndex);
}

Accessible*
EmbeddedObjCollector::EnsureNGetObject(uint32_t aIndex)
{
  uint32_t childCount = mRoot->ChildCount();
  while (mRootChildIdx < childCount) {
    Accessible* child = mRoot->GetChildAt(mRootChildIdx++);
    if (child->IsText())
      continue;

    AppendObject(child);
    if (mObjects.Length() - 1 == aIndex)
      return mObjects[aIndex];
  }

  return nullptr;
}

int32_t
EmbeddedObjCollector::EnsureNGetIndex(Accessible* aAccessible)
{
  uint32_t childCount = mRoot->ChildCount();
  while (mRootChildIdx < childCount) {
    Accessible* child = mRoot->GetChildAt(mRootChildIdx++);
    if (child->IsText())
      continue;

    AppendObject(child);
    if (child == aAccessible)
      return mObjects.Length() - 1;
  }

  return -1;
}

int32_t
EmbeddedObjCollector::GetIndexAt(Accessible* aAccessible)
{
  if (aAccessible->mParent != mRoot)
    return -1;

  MOZ_ASSERT(!aAccessible->IsProxy());
  if (aAccessible->mInt.mIndexOfEmbeddedChild != -1)
    return aAccessible->mInt.mIndexOfEmbeddedChild;

  return !aAccessible->IsText() ?  EnsureNGetIndex(aAccessible) : -1;
}

void
EmbeddedObjCollector::AppendObject(Accessible* aAccessible)
{
  MOZ_ASSERT(!aAccessible->IsProxy());
  aAccessible->mInt.mIndexOfEmbeddedChild = mObjects.Length();
  mObjects.AppendElement(aAccessible);
}
