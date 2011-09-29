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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#ifndef MOZILLA_SVGPOINTLIST_H__
#define MOZILLA_SVGPOINTLIST_H__

#include "SVGPoint.h"
#include "nsTArray.h"
#include "nsSVGElement.h"
#include "nsIWeakReferenceUtils.h"

namespace mozilla {

/**
 * ATTENTION! WARNING! WATCH OUT!!
 *
 * Consumers that modify objects of this type absolutely MUST keep the DOM
 * wrappers for those lists (if any) in sync!! That's why this class is so
 * locked down.
 *
 * The DOM wrapper class for this class is DOMSVGPointList.
 */
class SVGPointList
{
  friend class SVGAnimatedPointList;
  friend class DOMSVGPointList;
  friend class DOMSVGPoint;

public:

  SVGPointList(){}
  ~SVGPointList(){}

  // Only methods that don't make/permit modification to this list are public.
  // Only our friend classes can access methods that may change us.

  /// This may return an incomplete string on OOM, but that's acceptable.
  void GetValueAsString(nsAString& aValue) const;

  bool IsEmpty() const {
    return mItems.IsEmpty();
  }

  PRUint32 Length() const {
    return mItems.Length();
  }

  const SVGPoint& operator[](PRUint32 aIndex) const {
    return mItems[aIndex];
  }

  bool operator==(const SVGPointList& rhs) const {
    // memcmp can be faster than |mItems == rhs.mItems|
    return mItems.Length() == rhs.mItems.Length() &&
           memcmp(mItems.Elements(), rhs.mItems.Elements(),
                  mItems.Length() * sizeof(SVGPoint)) == 0;
  }

  bool SetCapacity(PRUint32 aSize) {
    return mItems.SetCapacity(aSize);
  }

  void Compact() {
    mItems.Compact();
  }

  // Access to methods that can modify objects of this type is deliberately
  // limited. This is to reduce the chances of someone modifying objects of
  // this type without taking the necessary steps to keep DOM wrappers in sync.
  // If you need wider access to these methods, consider adding a method to
  // SVGAnimatedPointList and having that class act as an intermediary so it
  // can take care of keeping DOM wrappers in sync.

protected:

  /**
   * This may fail on OOM if the internal capacity needs to be increased, in
   * which case the list will be left unmodified.
   */
  nsresult CopyFrom(const SVGPointList& rhs);

  SVGPoint& operator[](PRUint32 aIndex) {
    return mItems[aIndex];
  }

  /**
   * This may fail (return PR_FALSE) on OOM if the internal capacity is being
   * increased, in which case the list will be left unmodified.
   */
  bool SetLength(PRUint32 aNumberOfItems) {
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

  bool InsertItem(PRUint32 aIndex, const SVGPoint &aPoint) {
    if (aIndex >= mItems.Length()) {
      aIndex = mItems.Length();
    }
    return !!mItems.InsertElementAt(aIndex, aPoint);
  }

  void ReplaceItem(PRUint32 aIndex, const SVGPoint &aPoint) {
    NS_ABORT_IF_FALSE(aIndex < mItems.Length(),
                      "DOM wrapper caller should have raised INDEX_SIZE_ERR");
    mItems[aIndex] = aPoint;
  }

  void RemoveItem(PRUint32 aIndex) {
    NS_ABORT_IF_FALSE(aIndex < mItems.Length(),
                      "DOM wrapper caller should have raised INDEX_SIZE_ERR");
    mItems.RemoveElementAt(aIndex);
  }

  bool AppendItem(SVGPoint aPoint) {
    return !!mItems.AppendElement(aPoint);
  }

protected:

  /* See SVGLengthList for the rationale for using nsTArray<SVGPoint> instead
   * of nsTArray<SVGPoint, 1>.
   */
  nsTArray<SVGPoint> mItems;
};


/**
 * This SVGPointList subclass is for SVGPointListSMILType which needs a
 * mutable version of SVGPointList. Instances of this class do not have
 * DOM wrappers that need to be kept in sync, so we can safely expose any
 * protected base class methods required by the SMIL code.
 *
 * This class contains a strong reference to the element that instances of
 * this class are being used to animate. This is because the SMIL code stores
 * instances of this class in nsSMILValue objects, some of which are cached.
 * Holding a strong reference to the element here prevents the element from
 * disappearing out from under the SMIL code unexpectedly.
 */
class SVGPointListAndInfo : public SVGPointList
{
public:

  SVGPointListAndInfo(nsSVGElement *aElement = nsnull)
    : mElement(do_GetWeakReference(static_cast<nsINode*>(aElement)))
  {}

  void SetInfo(nsSVGElement *aElement) {
    mElement = do_GetWeakReference(static_cast<nsINode*>(aElement));
  }

  nsSVGElement* Element() const {
    nsCOMPtr<nsIContent> e = do_QueryReferent(mElement);
    return static_cast<nsSVGElement*>(e.get());
  }

  nsresult CopyFrom(const SVGPointListAndInfo& rhs) {
    mElement = rhs.mElement;
    return SVGPointList::CopyFrom(rhs);
  }

  /**
   * Exposed so that SVGPointList baseVals can be copied to
   * SVGPointListAndInfo objects. Note that callers should also call
   * SetElement() when using this method!
   */
  nsresult CopyFrom(const SVGPointList& rhs) {
    return SVGPointList::CopyFrom(rhs);
  }
  const SVGPoint& operator[](PRUint32 aIndex) const {
    return SVGPointList::operator[](aIndex);
  }
  SVGPoint& operator[](PRUint32 aIndex) {
    return SVGPointList::operator[](aIndex);
  }
  bool SetLength(PRUint32 aNumberOfItems) {
    return SVGPointList::SetLength(aNumberOfItems);
  }

private:
  // We must keep a weak reference to our element because we may belong to a
  // cached baseVal nsSMILValue. See the comments starting at:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=515116#c15
  // See also https://bugzilla.mozilla.org/show_bug.cgi?id=653497
  nsWeakPtr mElement;
};

} // namespace mozilla

#endif // MOZILLA_SVGPOINTLIST_H__
