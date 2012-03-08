/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "FramePropertyTable.h"
#include "prlog.h"

namespace mozilla {

void
FramePropertyTable::Set(nsIFrame* aFrame, const FramePropertyDescriptor* aProperty,
                        void* aValue)
{
  NS_ASSERTION(aFrame, "Null frame?");
  NS_ASSERTION(aProperty, "Null property?");

  if (mLastFrame != aFrame || !mLastEntry) {
    mLastFrame = aFrame;
    mLastEntry = mEntries.PutEntry(aFrame);
  }
  Entry* entry = mLastEntry;

  if (!entry->mProp.IsArray()) {
    if (!entry->mProp.mProperty) {
      // Empty entry, so we can just store our property in the empty slot
      entry->mProp.mProperty = aProperty;
      entry->mProp.mValue = aValue;
      return;
    }
    if (entry->mProp.mProperty == aProperty) {
      // Just overwrite the current value
      entry->mProp.DestroyValueFor(aFrame);
      entry->mProp.mValue = aValue;
      return;
    }

    // We need to expand the single current entry to an array
    PropertyValue current = entry->mProp;
    entry->mProp.mProperty = nsnull;
    PR_STATIC_ASSERT(sizeof(nsTArray<PropertyValue>) <= sizeof(void *));
    new (&entry->mProp.mValue) nsTArray<PropertyValue>(4);
    entry->mProp.ToArray()->AppendElement(current);
  }

  nsTArray<PropertyValue>* array = entry->mProp.ToArray();
  nsTArray<PropertyValue>::index_type index =
    array->IndexOf(aProperty, 0, PropertyComparator());
  if (index != nsTArray<PropertyValue>::NoIndex) {
    PropertyValue* pv = &array->ElementAt(index);
    pv->DestroyValueFor(aFrame);
    pv->mValue = aValue;
    return;
  }

  array->AppendElement(PropertyValue(aProperty, aValue));
}

void*
FramePropertyTable::Get(const nsIFrame* aFrame,
                        const FramePropertyDescriptor* aProperty,
                        bool* aFoundResult)
{
  NS_ASSERTION(aFrame, "Null frame?");
  NS_ASSERTION(aProperty, "Null property?");

  if (aFoundResult) {
    *aFoundResult = false;
  }

  if (mLastFrame != aFrame) {
    mLastFrame = const_cast<nsIFrame*>(aFrame);
    mLastEntry = mEntries.GetEntry(mLastFrame);
  }
  Entry* entry = mLastEntry;
  if (!entry)
    return nsnull;

  if (entry->mProp.mProperty == aProperty) {
    if (aFoundResult) {
      *aFoundResult = true;
    }
    return entry->mProp.mValue;
  }
  if (!entry->mProp.IsArray()) {
    // There's just one property and it's not the one we want, bail
    return nsnull;
  }

  nsTArray<PropertyValue>* array = entry->mProp.ToArray();
  nsTArray<PropertyValue>::index_type index =
    array->IndexOf(aProperty, 0, PropertyComparator());
  if (index == nsTArray<PropertyValue>::NoIndex)
    return nsnull;

  if (aFoundResult) {
    *aFoundResult = true;
  }

  return array->ElementAt(index).mValue;
}

void*
FramePropertyTable::Remove(nsIFrame* aFrame, const FramePropertyDescriptor* aProperty,
                           bool* aFoundResult)
{
  NS_ASSERTION(aFrame, "Null frame?");
  NS_ASSERTION(aProperty, "Null property?");

  if (aFoundResult) {
    *aFoundResult = false;
  }

  if (mLastFrame != aFrame) {
    mLastFrame = aFrame;
    mLastEntry = mEntries.GetEntry(aFrame);
  }
  Entry* entry = mLastEntry;
  if (!entry)
    return nsnull;

  if (entry->mProp.mProperty == aProperty) {
    // There's only one entry and it's the one we want
    void* value = entry->mProp.mValue;
    mEntries.RawRemoveEntry(entry);
    mLastEntry = nsnull;
    if (aFoundResult) {
      *aFoundResult = true;
    }
    return value;
  }
  if (!entry->mProp.IsArray()) {
    // There's just one property and it's not the one we want, bail
    return nsnull;
  }

  nsTArray<PropertyValue>* array = entry->mProp.ToArray();
  nsTArray<PropertyValue>::index_type index =
    array->IndexOf(aProperty, 0, PropertyComparator());
  if (index == nsTArray<PropertyValue>::NoIndex) {
    // No such property, bail
    return nsnull;
  }

  if (aFoundResult) {
    *aFoundResult = true;
  }

  void* result = array->ElementAt(index).mValue;

  PRUint32 last = array->Length() - 1;
  array->ElementAt(index) = array->ElementAt(last);
  array->RemoveElementAt(last);

  if (last == 1) {
    PropertyValue pv = array->ElementAt(0);
    array->~nsTArray<PropertyValue>();
    entry->mProp = pv;
  }
  
  return result;
}

void
FramePropertyTable::Delete(nsIFrame* aFrame, const FramePropertyDescriptor* aProperty)
{
  NS_ASSERTION(aFrame, "Null frame?");
  NS_ASSERTION(aProperty, "Null property?");

  bool found;
  void* v = Remove(aFrame, aProperty, &found);
  if (found) {
    PropertyValue pv(aProperty, v);
    pv.DestroyValueFor(aFrame);
  }
}

/* static */ void
FramePropertyTable::DeleteAllForEntry(Entry* aEntry)
{
  if (!aEntry->mProp.IsArray()) {
    aEntry->mProp.DestroyValueFor(aEntry->GetKey());
    return;
  }

  nsTArray<PropertyValue>* array = aEntry->mProp.ToArray();
  for (PRUint32 i = 0; i < array->Length(); ++i) {
    array->ElementAt(i).DestroyValueFor(aEntry->GetKey());
  }
  array->~nsTArray<PropertyValue>();
}

void
FramePropertyTable::DeleteAllFor(nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame, "Null frame?");

  Entry* entry = mEntries.GetEntry(aFrame);
  if (!entry)
    return;

  if (mLastFrame == aFrame) {
    // Flush cache. We assume DeleteAllForEntry will be called before
    // a frame is destroyed.
    mLastFrame = nsnull;
    mLastEntry = nsnull;
  }

  DeleteAllForEntry(entry);
  mEntries.RawRemoveEntry(entry);
}

/* static */ PLDHashOperator
FramePropertyTable::DeleteEnumerator(Entry* aEntry, void* aArg)
{
  DeleteAllForEntry(aEntry);
  return PL_DHASH_REMOVE;
}

void
FramePropertyTable::DeleteAll()
{
  mLastFrame = nsnull;
  mLastEntry = nsnull;

  mEntries.EnumerateEntries(DeleteEnumerator, nsnull);
}

size_t
FramePropertyTable::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return mEntries.SizeOfExcludingThis(SizeOfPropertyTableEntryExcludingThis,
                                      aMallocSizeOf);
}

/* static */ size_t
FramePropertyTable::SizeOfPropertyTableEntryExcludingThis(Entry* aEntry,
                      nsMallocSizeOfFun aMallocSizeOf, void *)
{
  return aEntry->mProp.SizeOfExcludingThis(aMallocSizeOf);
}

}
