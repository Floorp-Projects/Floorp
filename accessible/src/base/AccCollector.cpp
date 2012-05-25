/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccCollector.h"

#include "nsAccessible.h"

////////////////////////////////////////////////////////////////////////////////
// nsAccCollector
////////////////////////////////////////////////////////////////////////////////

AccCollector::
  AccCollector(nsAccessible* aRoot, filters::FilterFuncPtr aFilterFunc) :
  mFilterFunc(aFilterFunc), mRoot(aRoot), mRootChildIdx(0)
{
}

AccCollector::~AccCollector()
{
}

PRUint32
AccCollector::Count()
{
  EnsureNGetIndex(nsnull);
  return mObjects.Length();
}

nsAccessible*
AccCollector::GetAccessibleAt(PRUint32 aIndex)
{
  nsAccessible *accessible = mObjects.SafeElementAt(aIndex, nsnull);
  if (accessible)
    return accessible;

  return EnsureNGetObject(aIndex);
}

PRInt32
AccCollector::GetIndexAt(nsAccessible *aAccessible)
{
  PRInt32 index = mObjects.IndexOf(aAccessible);
  if (index != -1)
    return index;

  return EnsureNGetIndex(aAccessible);
}

////////////////////////////////////////////////////////////////////////////////
// nsAccCollector protected

nsAccessible*
AccCollector::EnsureNGetObject(PRUint32 aIndex)
{
  PRUint32 childCount = mRoot->ChildCount();
  while (mRootChildIdx < childCount) {
    nsAccessible* child = mRoot->GetChildAt(mRootChildIdx++);
    if (!mFilterFunc(child))
      continue;

    AppendObject(child);
    if (mObjects.Length() - 1 == aIndex)
      return mObjects[aIndex];
  }

  return nsnull;
}

PRInt32
AccCollector::EnsureNGetIndex(nsAccessible* aAccessible)
{
  PRUint32 childCount = mRoot->ChildCount();
  while (mRootChildIdx < childCount) {
    nsAccessible* child = mRoot->GetChildAt(mRootChildIdx++);
    if (!mFilterFunc(child))
      continue;

    AppendObject(child);
    if (child == aAccessible)
      return mObjects.Length() - 1;
  }

  return -1;
}

void
AccCollector::AppendObject(nsAccessible* aAccessible)
{
  mObjects.AppendElement(aAccessible);
}

////////////////////////////////////////////////////////////////////////////////
// EmbeddedObjCollector
////////////////////////////////////////////////////////////////////////////////

PRInt32
EmbeddedObjCollector::GetIndexAt(nsAccessible *aAccessible)
{
  if (aAccessible->mParent != mRoot)
    return -1;

  if (aAccessible->mIndexOfEmbeddedChild != -1)
    return aAccessible->mIndexOfEmbeddedChild;

  return mFilterFunc(aAccessible) ? EnsureNGetIndex(aAccessible) : -1;
}

void
EmbeddedObjCollector::AppendObject(nsAccessible* aAccessible)
{
  aAccessible->mIndexOfEmbeddedChild = mObjects.Length();
  mObjects.AppendElement(aAccessible);
}
