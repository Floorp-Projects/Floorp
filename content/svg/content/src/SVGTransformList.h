/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
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
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
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

#ifndef MOZILLA_SVGTRANSFORMLIST_H__
#define MOZILLA_SVGTRANSFORMLIST_H__

#include "SVGTransform.h"
#include "nsTArray.h"
#include "nsSVGElement.h"

namespace mozilla {

/**
 * ATTENTION! WARNING! WATCH OUT!!
 *
 * Consumers that modify objects of this type absolutely MUST keep the DOM
 * wrappers for those lists (if any) in sync!! That's why this class is so
 * locked down.
 *
 * The DOM wrapper class for this class is DOMSVGTransformList.
 */
class SVGTransformList
{
  friend class SVGAnimatedTransformList;
  friend class DOMSVGTransformList;
  friend class DOMSVGTransform;

public:
  SVGTransformList() {}
  ~SVGTransformList() {}

  // Only methods that don't make/permit modification to this list are public.
  // Only our friend classes can access methods that may change us.

  /// This may return an incomplete string on OOM, but that's acceptable.
  void GetValueAsString(nsAString& aValue) const;

  PRBool IsEmpty() const {
    return mItems.IsEmpty();
  }

  PRUint32 Length() const {
    return mItems.Length();
  }

  const SVGTransform& operator[](PRUint32 aIndex) const {
    return mItems[aIndex];
  }

  PRBool operator==(const SVGTransformList& rhs) const {
    return mItems == rhs.mItems;
  }

  PRBool SetCapacity(PRUint32 size) {
    return mItems.SetCapacity(size);
  }

  void Compact() {
    mItems.Compact();
  }

  gfxMatrix GetConsolidationMatrix() const;

  // Access to methods that can modify objects of this type is deliberately
  // limited. This is to reduce the chances of someone modifying objects of
  // this type without taking the necessary steps to keep DOM wrappers in sync.
  // If you need wider access to these methods, consider adding a method to
  // SVGAnimatedTransformList and having that class act as an intermediary so it
  // can take care of keeping DOM wrappers in sync.

protected:

  /**
   * These may fail on OOM if the internal capacity needs to be increased, in
   * which case the list will be left unmodified.
   */
  nsresult CopyFrom(const SVGTransformList& rhs);
  nsresult CopyFrom(const nsTArray<SVGTransform>& aTransformArray);

  SVGTransform& operator[](PRUint32 aIndex) {
    return mItems[aIndex];
  }

  /**
   * This may fail (return PR_FALSE) on OOM if the internal capacity is being
   * increased, in which case the list will be left unmodified.
   */
  PRBool SetLength(PRUint32 aNumberOfItems) {
    return mItems.SetLength(aNumberOfItems);
  }

private:

  // Marking the following private only serves to show which methods are only
  // used by our friend classes (as opposed to our subclasses) - it doesn't
  // really provide additional safety.

  nsresult SetValueFromString(const nsAString& aValue);

  void Clear() {
    mItems.Clear();
  }

  PRBool InsertItem(PRUint32 aIndex, const SVGTransform& aTransform) {
    if (aIndex >= mItems.Length()) {
      aIndex = mItems.Length();
    }
    return !!mItems.InsertElementAt(aIndex, aTransform);
  }

  void ReplaceItem(PRUint32 aIndex, const SVGTransform& aTransform) {
    NS_ABORT_IF_FALSE(aIndex < mItems.Length(),
                      "DOM wrapper caller should have raised INDEX_SIZE_ERR");
    mItems[aIndex] = aTransform;
  }

  void RemoveItem(PRUint32 aIndex) {
    NS_ABORT_IF_FALSE(aIndex < mItems.Length(),
                      "DOM wrapper caller should have raised INDEX_SIZE_ERR");
    mItems.RemoveElementAt(aIndex);
  }

  PRBool AppendItem(const SVGTransform& aTransform) {
    return !!mItems.AppendElement(aTransform);
  }

protected:
  /*
   * See SVGLengthList for the rationale for using nsTArray<SVGTransform>
   * instead of nsTArray<SVGTransform, 1>.
   */
  nsTArray<SVGTransform> mItems;
};

} // namespace mozilla

#endif // MOZILLA_SVGTRANSFORMLIST_H__
