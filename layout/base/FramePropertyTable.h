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

#ifndef FRAMEPROPERTYTABLE_H_
#define FRAMEPROPERTYTABLE_H_

#include "nsTHashtable.h"
#include "nsHashKeys.h"

class nsIFrame;

namespace mozilla {

struct FramePropertyDescriptor;

typedef void (*FramePropertyDestructor)(void* aPropertyValue);
typedef void (*FramePropertyDestructorWithFrame)(nsIFrame* aFrame,
                                                 void* aPropertyValue);

/**
 * A pointer to a FramePropertyDescriptor serves as a unique property ID.
 * The FramePropertyDescriptor stores metadata about the property.
 * Currently the only metadata is a destructor function. The destructor
 * function is called on property values when they are overwritten or
 * deleted.
 * 
 * To use this class, declare a global (i.e., file, class or function-scope
 * static member) FramePropertyDescriptor and pass its address as
 * aProperty in the FramePropertyTable methods.
 */
struct FramePropertyDescriptor {
  /**
   * mDestructor will be called if it's non-null.
   */
  FramePropertyDestructor          mDestructor;
  /**
   * mDestructorWithFrame will be called if it's non-null and mDestructor
   * is null. WARNING: The frame passed to mDestructorWithFrame may
   * be a dangling frame pointer, if this is being called during
   * presshell teardown. Do not use it except to compare against
   * other frame pointers. No frame will have been allocated with
   * the same address yet.
   */
  FramePropertyDestructorWithFrame mDestructorWithFrame;
  /**
   * mDestructor and mDestructorWithFrame may both be null, in which case
   * no value destruction is a no-op.
   */
};

/**
 * The FramePropertyTable is optimized for storing 0 or 1 properties on
 * a given frame. Storing very large numbers of properties on a single
 * frame will not be efficient.
 * 
 * Property values are passed as void* but do not actually have to be
 * valid pointers. You can use NS_INT32_TO_PTR/NS_PTR_TO_INT32 to
 * store PRInt32 values. Null/zero values can be stored and retrieved.
 * Of course, the destructor function (if any) must handle such values
 * correctly.
 */
class FramePropertyTable {
public:
  FramePropertyTable() : mLastFrame(nsnull), mLastEntry(nsnull)
  {
    mEntries.Init();
  }
  ~FramePropertyTable()
  {
    DeleteAll();
  }

  /**
   * Set a property value on a frame. This requires one hashtable
   * lookup (using the frame as the key) and a linear search through
   * the properties of that frame. Any existing value for the property
   * is destroyed.
   */
  void Set(nsIFrame* aFrame, const FramePropertyDescriptor* aProperty,
           void* aValue);
  /**
   * Get a property value for a frame. This requires one hashtable
   * lookup (using the frame as the key) and a linear search through
   * the properties of that frame. If the frame has no such property,
   * returns null.
   * @param aFoundResult if non-null, receives a value 'true' iff
   * the frame has a value for the property. This lets callers
   * disambiguate a null result, which can mean 'no such property' or
   * 'property value is null'.
   */
  void* Get(const nsIFrame* aFrame, const FramePropertyDescriptor* aProperty,
            bool* aFoundResult = nsnull);
  /**
   * Remove a property value for a frame. This requires one hashtable
   * lookup (using the frame as the key) and a linear search through
   * the properties of that frame. The old property value is returned
   * (and not destroyed). If the frame has no such property,
   * returns null.
   * @param aFoundResult if non-null, receives a value 'true' iff
   * the frame had a value for the property. This lets callers
   * disambiguate a null result, which can mean 'no such property' or
   * 'property value is null'.
   */
  void* Remove(nsIFrame* aFrame, const FramePropertyDescriptor* aProperty,
               bool* aFoundResult = nsnull);
  /**
   * Remove and destroy a property value for a frame. This requires one
   * hashtable lookup (using the frame as the key) and a linear search
   * through the properties of that frame. If the frame has no such
   * property, nothing happens.
   */
  void Delete(nsIFrame* aFrame, const FramePropertyDescriptor* aProperty);
  /**
   * Remove and destroy all property values for a frame. This requires one
   * hashtable lookup (using the frame as the key).
   */
  void DeleteAllFor(nsIFrame* aFrame);
  /**
   * Remove and destroy all property values for all frames.
   */
  void DeleteAll();

protected:
  /**
   * Stores a property descriptor/value pair. It can also be used to
   * store an nsTArray of PropertyValues.
   */
  struct PropertyValue {
    PropertyValue() : mProperty(nsnull), mValue(nsnull) {}
    PropertyValue(const FramePropertyDescriptor* aProperty, void* aValue)
      : mProperty(aProperty), mValue(aValue) {}

    bool IsArray() { return !mProperty && mValue; }
    nsTArray<PropertyValue>* ToArray()
    {
      NS_ASSERTION(IsArray(), "Must be array");
      return reinterpret_cast<nsTArray<PropertyValue>*>(&mValue);
    }

    void DestroyValueFor(nsIFrame* aFrame) {
      if (mProperty->mDestructor) {
        mProperty->mDestructor(mValue);
      } else if (mProperty->mDestructorWithFrame) {
        mProperty->mDestructorWithFrame(aFrame, mValue);
      }
    }

    const FramePropertyDescriptor* mProperty;
    void* mValue;
  };

  /**
   * Used with an array of PropertyValues to allow lookups that compare
   * only on the FramePropertyDescriptor.
   */
  class PropertyComparator {
  public:
    bool Equals(const PropertyValue& a, const PropertyValue& b) const {
      return a.mProperty == b.mProperty;
    }
    bool Equals(const FramePropertyDescriptor* a, const PropertyValue& b) const {
      return a == b.mProperty;
    }
    bool Equals(const PropertyValue& a, const FramePropertyDescriptor* b) const {
      return a.mProperty == b;
    }
  };

  /**
   * Our hashtable entry. The key is an nsIFrame*, the value is a
   * PropertyValue representing one or more property/value pairs.
   */
  class Entry : public nsPtrHashKey<nsIFrame>
  {
  public:
    Entry(KeyTypePointer aKey) : nsPtrHashKey<nsIFrame>(aKey) {}
    Entry(const Entry &toCopy) :
      nsPtrHashKey<nsIFrame>(toCopy), mProp(toCopy.mProp) {}

    PropertyValue mProp;
  };

  static void DeleteAllForEntry(Entry* aEntry);
  static PLDHashOperator DeleteEnumerator(Entry* aEntry, void* aArg);

  nsTHashtable<Entry> mEntries;
  nsIFrame* mLastFrame;
  Entry* mLastEntry;
};

/**
 * This class encapsulates the properties of a frame.
 */
class FrameProperties {
public:
  FrameProperties(FramePropertyTable* aTable, nsIFrame* aFrame)
    : mTable(aTable), mFrame(aFrame) {}
  FrameProperties(FramePropertyTable* aTable, const nsIFrame* aFrame)
    : mTable(aTable), mFrame(const_cast<nsIFrame*>(aFrame)) {}

  void Set(const FramePropertyDescriptor* aProperty, void* aValue) const
  {
    mTable->Set(mFrame, aProperty, aValue);
  }
  void* Get(const FramePropertyDescriptor* aProperty,
            bool* aFoundResult = nsnull) const
  {
    return mTable->Get(mFrame, aProperty, aFoundResult);
  }
  void* Remove(const FramePropertyDescriptor* aProperty,
               bool* aFoundResult = nsnull) const
  {
    return mTable->Remove(mFrame, aProperty, aFoundResult);
  }
  void Delete(const FramePropertyDescriptor* aProperty)
  {
    mTable->Delete(mFrame, aProperty);
  }

private:
  FramePropertyTable* mTable;
  nsIFrame* mFrame;
};

}

#endif /* FRAMEPROPERTYTABLE_H_ */
