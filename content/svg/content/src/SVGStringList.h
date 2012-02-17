/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is
 * Robert Longson <longsonr@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

  PRUint32 Length() const {
    return mStrings.Length();
  }

  const nsAString& operator[](PRUint32 aIndex) const {
    return mStrings[aIndex];
  }

  bool operator==(const SVGStringList& rhs) const {
    return mStrings == rhs.mStrings;
  }

  bool SetCapacity(PRUint32 size) {
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

  nsAString& operator[](PRUint32 aIndex) {
    return mStrings[aIndex];
  }

  /**
   * This may fail (return false) on OOM if the internal capacity is being
   * increased, in which case the list will be left unmodified.
   */
  bool SetLength(PRUint32 aStringOfItems) {
    return mStrings.SetLength(aStringOfItems);
  }

private:

  // Marking the following private only serves to show which methods are only
  // used by our friend classes (as opposed to our subclasses) - it doesn't
  // really provide additional safety.

  bool InsertItem(PRUint32 aIndex, const nsAString &aString) {
    if (aIndex >= mStrings.Length()) {
      aIndex = mStrings.Length();
    }
    if (mStrings.InsertElementAt(aIndex, aString)) {
      mIsSet = true;
      return true;
    }
    return false;
  }

  void ReplaceItem(PRUint32 aIndex, const nsAString &aString) {
    NS_ABORT_IF_FALSE(aIndex < mStrings.Length(),
                      "DOM wrapper caller should have raised INDEX_SIZE_ERR");
    mStrings[aIndex] = aString;
  }

  void RemoveItem(PRUint32 aIndex) {
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

  /* See SVGLengthList for the rationale for using nsTArray<float> instead
   * of nsTArray<float, 1>.
   */
  nsTArray<nsString> mStrings;
  bool mIsSet;
  bool mIsCommaSeparated;
};

} // namespace mozilla

#endif // MOZILLA_SVGSTRINGLIST_H__
