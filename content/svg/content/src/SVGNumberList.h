/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGNUMBERLIST_H__
#define MOZILLA_SVGNUMBERLIST_H__

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIContent.h"
#include "nsINode.h"
#include "nsIWeakReferenceUtils.h"
#include "nsSVGElement.h"
#include "nsTArray.h"

namespace mozilla {

/**
 * ATTENTION! WARNING! WATCH OUT!!
 *
 * Consumers that modify objects of this type absolutely MUST keep the DOM
 * wrappers for those lists (if any) in sync!! That's why this class is so
 * locked down.
 *
 * The DOM wrapper class for this class is DOMSVGNumberList.
 */
class SVGNumberList
{
  friend class SVGAnimatedNumberList;
  friend class DOMSVGNumberList;
  friend class DOMSVGNumber;

public:

  SVGNumberList(){}
  ~SVGNumberList(){}

  // Only methods that don't make/permit modification to this list are public.
  // Only our friend classes can access methods that may change us.

  /// This may return an incomplete string on OOM, but that's acceptable.
  void GetValueAsString(nsAString& aValue) const;

  bool IsEmpty() const {
    return mNumbers.IsEmpty();
  }

  PRUint32 Length() const {
    return mNumbers.Length();
  }

  const float& operator[](PRUint32 aIndex) const {
    return mNumbers[aIndex];
  }

  bool operator==(const SVGNumberList& rhs) const {
    return mNumbers == rhs.mNumbers;
  }

  bool SetCapacity(PRUint32 size) {
    return mNumbers.SetCapacity(size);
  }

  void Compact() {
    mNumbers.Compact();
  }

  // Access to methods that can modify objects of this type is deliberately
  // limited. This is to reduce the chances of someone modifying objects of
  // this type without taking the necessary steps to keep DOM wrappers in sync.
  // If you need wider access to these methods, consider adding a method to
  // SVGAnimatedNumberList and having that class act as an intermediary so it
  // can take care of keeping DOM wrappers in sync.

protected:

  /**
   * This may fail on OOM if the internal capacity needs to be increased, in
   * which case the list will be left unmodified.
   */
  nsresult CopyFrom(const SVGNumberList& rhs);

  float& operator[](PRUint32 aIndex) {
    return mNumbers[aIndex];
  }

  /**
   * This may fail (return false) on OOM if the internal capacity is being
   * increased, in which case the list will be left unmodified.
   */
  bool SetLength(PRUint32 aNumberOfItems) {
    return mNumbers.SetLength(aNumberOfItems);
  }

private:

  // Marking the following private only serves to show which methods are only
  // used by our friend classes (as opposed to our subclasses) - it doesn't
  // really provide additional safety.

  nsresult SetValueFromString(const nsAString& aValue);

  void Clear() {
    mNumbers.Clear();
  }

  bool InsertItem(PRUint32 aIndex, const float &aNumber) {
    if (aIndex >= mNumbers.Length()) {
      aIndex = mNumbers.Length();
    }
    return !!mNumbers.InsertElementAt(aIndex, aNumber);
  }

  void ReplaceItem(PRUint32 aIndex, const float &aNumber) {
    NS_ABORT_IF_FALSE(aIndex < mNumbers.Length(),
                      "DOM wrapper caller should have raised INDEX_SIZE_ERR");
    mNumbers[aIndex] = aNumber;
  }

  void RemoveItem(PRUint32 aIndex) {
    NS_ABORT_IF_FALSE(aIndex < mNumbers.Length(),
                      "DOM wrapper caller should have raised INDEX_SIZE_ERR");
    mNumbers.RemoveElementAt(aIndex);
  }

  bool AppendItem(float aNumber) {
    return !!mNumbers.AppendElement(aNumber);
  }

protected:

  /* See SVGLengthList for the rationale for using nsTArray<float> instead
   * of nsTArray<float, 1>.
   */
  nsTArray<float> mNumbers;
};


/**
 * This SVGNumberList subclass is used by the SMIL code when a number list
 * is to be stored in an nsISMILValue instance. Since nsISMILValue objects may
 * be cached, it is necessary for us to hold a strong reference to our element
 * so that it doesn't disappear out from under us if, say, the element is
 * removed from the DOM tree.
 */
class SVGNumberListAndInfo : public SVGNumberList
{
public:

  SVGNumberListAndInfo()
    : mElement(nullptr)
  {}

  SVGNumberListAndInfo(nsSVGElement *aElement)
    : mElement(do_GetWeakReference(static_cast<nsINode*>(aElement)))
  {}

  void SetInfo(nsSVGElement *aElement) {
    mElement = do_GetWeakReference(static_cast<nsINode*>(aElement));
  }

  nsSVGElement* Element() const {
    nsCOMPtr<nsIContent> e = do_QueryReferent(mElement);
    return static_cast<nsSVGElement*>(e.get());
  }

  nsresult CopyFrom(const SVGNumberListAndInfo& rhs) {
    mElement = rhs.mElement;
    return SVGNumberList::CopyFrom(rhs);
  }

  // Instances of this special subclass do not have DOM wrappers that we need
  // to worry about keeping in sync, so it's safe to expose any hidden base
  // class methods required by the SMIL code, as we do below.

  /**
   * Exposed so that SVGNumberList baseVals can be copied to
   * SVGNumberListAndInfo objects. Note that callers should also call
   * SetInfo() when using this method!
   */
  nsresult CopyFrom(const SVGNumberList& rhs) {
    return SVGNumberList::CopyFrom(rhs);
  }
  const float& operator[](PRUint32 aIndex) const {
    return SVGNumberList::operator[](aIndex);
  }
  float& operator[](PRUint32 aIndex) {
    return SVGNumberList::operator[](aIndex);
  }
  bool SetLength(PRUint32 aNumberOfItems) {
    return SVGNumberList::SetLength(aNumberOfItems);
  }

private:
  // We must keep a weak reference to our element because we may belong to a
  // cached baseVal nsSMILValue. See the comments starting at:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=515116#c15
  // See also https://bugzilla.mozilla.org/show_bug.cgi?id=653497
  nsWeakPtr mElement;
};

} // namespace mozilla

#endif // MOZILLA_SVGNUMBERLIST_H__
