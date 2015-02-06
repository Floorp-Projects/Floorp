/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGSTRINGLIST_H__
#define MOZILLA_SVGSTRINGLIST_H__

#include "nsDebug.h"
#include "nsTArray.h"
#include "nsString.h" // IWYU pragma: keep

namespace mozilla {

/**
 *
 * The DOM wrapper class for this class is DOMSVGStringList.
 */
class SVGStringList
{
  friend class DOMSVGStringList;

public:

  SVGStringList() : mIsSet(false), mIsCommaSeparated(false) {}
  ~SVGStringList(){}

  void SetIsCommaSeparated(bool aIsCommaSeparated) {
    mIsCommaSeparated = aIsCommaSeparated;
  }
  nsresult SetValue(const nsAString& aValue);

  void Clear() {
    mStrings.Clear();
    mIsSet = false;
  }

  /// This may return an incomplete string on OOM, but that's acceptable.
  void GetValue(nsAString& aValue) const;

  bool IsEmpty() const {
    return mStrings.IsEmpty();
  }

  uint32_t Length() const {
    return mStrings.Length();
  }

  const nsAString& operator[](uint32_t aIndex) const {
    return mStrings[aIndex];
  }

  bool operator==(const SVGStringList& rhs) const {
    return mStrings == rhs.mStrings;
  }

  bool SetCapacity(uint32_t size) {
    return mStrings.SetCapacity(size);
  }

  void Compact() {
    mStrings.Compact();
  }

  // Returns true if the value of this stringlist has been explicitly
  // set by markup or a DOM call, false otherwise.
  bool IsExplicitlySet() const
    { return mIsSet; }

  // Access to methods that can modify objects of this type is deliberately
  // limited. This is to reduce the chances of someone modifying objects of
  // this type without taking the necessary steps to keep DOM wrappers in sync.
  // If you need wider access to these methods, consider adding a method to
  // SVGAnimatedStringList and having that class act as an intermediary so it
  // can take care of keeping DOM wrappers in sync.

protected:

  /**
   * This may fail on OOM if the internal capacity needs to be increased, in
   * which case the list will be left unmodified.
   */
  nsresult CopyFrom(const SVGStringList& rhs);

  nsAString& operator[](uint32_t aIndex) {
    return mStrings[aIndex];
  }

  /**
   * This may fail (return false) on OOM if the internal capacity is being
   * increased, in which case the list will be left unmodified.
   */
  bool SetLength(uint32_t aStringOfItems) {
    return mStrings.SetLength(aStringOfItems);
  }

private:

  // Marking the following private only serves to show which methods are only
  // used by our friend classes (as opposed to our subclasses) - it doesn't
  // really provide additional safety.

  bool InsertItem(uint32_t aIndex, const nsAString &aString) {
    if (aIndex >= mStrings.Length()) {
      aIndex = mStrings.Length();
    }
    if (mStrings.InsertElementAt(aIndex, aString)) {
      mIsSet = true;
      return true;
    }
    return false;
  }

  void ReplaceItem(uint32_t aIndex, const nsAString &aString) {
    NS_ABORT_IF_FALSE(aIndex < mStrings.Length(),
                      "DOM wrapper caller should have raised INDEX_SIZE_ERR");
    mStrings[aIndex] = aString;
  }

  void RemoveItem(uint32_t aIndex) {
    NS_ABORT_IF_FALSE(aIndex < mStrings.Length(),
                      "DOM wrapper caller should have raised INDEX_SIZE_ERR");
    mStrings.RemoveElementAt(aIndex);
  }

  bool AppendItem(const nsAString &aString) {
    if (mStrings.AppendElement(aString)) {
      mIsSet = true;
      return true;
    }
    return false;
  }

protected:

  /* See SVGLengthList for the rationale for using FallibleTArray<float> instead
   * of FallibleTArray<float, 1>.
   */
  FallibleTArray<nsString> mStrings;
  bool mIsSet;
  bool mIsCommaSeparated;
};

} // namespace mozilla

#endif // MOZILLA_SVGSTRINGLIST_H__
