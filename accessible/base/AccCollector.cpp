/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccCollector.h"

#include "Accessible.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsAccCollector
////////////////////////////////////////////////////////////////////////////////

AccCollector::
  AccCollector(Accessible* aRoot, filters::FilterFuncPtr aFilterFunc) :
  mFilterFunc(aFilterFunc), mRoot(aRoot), mRootChildIdx(0)
{
}

AccCollector::~AccCollector()
{
}

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

////////////////////////////////////////////////////////////////////////////////
// nsAccCollector protected

Accessible*
EmbeddedObjCollector::EnsureNGetObject(uint32_t aIndex)
{
  uint32_t childCount = mRoot->ChildCount();
  while (mRootChildIdx < childCount) {
    Accessible* child = mRoot->GetChildAt(mRootChildIdx++);
    if (!(mFilterFunc(child) & filters::eMatch))
      continue;

    AppendObject(child);
    if (mObjects.Length() - 1 == aIndex)
      return mObjects[aIndex];
  }

  return nullptr;
}

int32_t
AccCollector::EnsureNGetIndex(Accessible* aAccessible)
{
  uint32_t childCount = mRoot->ChildCount();
  while (mRootChildIdx < childCount) {
    Accessible* child = mRoot->GetChildAt(mRootChildIdx++);
    if (!(mFilterFunc(child) & filters::eMatch))
      continue;

    AppendObject(child);
    if (child == aAccessible)
      return mObjects.Length() - 1;
  }

  return -1;
}

void
AccCollector::AppendObject(Accessible* aAccessible)
{
  mObjects.AppendElement(aAccessible);
}

////////////////////////////////////////////////////////////////////////////////
// EmbeddedObjCollector
////////////////////////////////////////////////////////////////////////////////

int32_t
EmbeddedObjCollector::GetIndexAt(Accessible* aAccessible)
{
  if (aAccessible->mParent != mRoot)
    return -1;

  MOZ_ASSERT(!aAccessible->IsProxy());
  if (aAccessible->mInt.mIndexOfEmbeddedChild != -1)
    return aAccessible->mInt.mIndexOfEmbeddedChild;

  return mFilterFunc(aAccessible) & filters::eMatch ?
    EnsureNGetIndex(aAccessible) : -1;
}

void
EmbeddedObjCollector::AppendObject(Accessible* aAccessible)
{
  MOZ_ASSERT(!aAccessible->IsProxy());
  aAccessible->mInt.mIndexOfEmbeddedChild = mObjects.Length();
  mObjects.AppendElement(aAccessible);
}
