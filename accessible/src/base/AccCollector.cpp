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
AccCollector::Count()
{
  EnsureNGetIndex(nullptr);
  return mObjects.Length();
}

Accessible*
AccCollector::GetAccessibleAt(uint32_t aIndex)
{
  Accessible* accessible = mObjects.SafeElementAt(aIndex, nullptr);
  if (accessible)
    return accessible;

  return EnsureNGetObject(aIndex);
}

int32_t
AccCollector::GetIndexAt(Accessible* aAccessible)
{
  int32_t index = mObjects.IndexOf(aAccessible);
  if (index != -1)
    return index;

  return EnsureNGetIndex(aAccessible);
}

////////////////////////////////////////////////////////////////////////////////
// nsAccCollector protected

Accessible*
AccCollector::EnsureNGetObject(uint32_t aIndex)
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

  if (aAccessible->mIndexOfEmbeddedChild != -1)
    return aAccessible->mIndexOfEmbeddedChild;

  return mFilterFunc(aAccessible) & filters::eMatch ?
    EnsureNGetIndex(aAccessible) : -1;
}

void
EmbeddedObjCollector::AppendObject(Accessible* aAccessible)
{
  aAccessible->mIndexOfEmbeddedChild = mObjects.Length();
  mObjects.AppendElement(aAccessible);
}
