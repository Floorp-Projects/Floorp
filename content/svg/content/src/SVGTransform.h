/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGTransform_h
#define mozilla_dom_SVGTransform_h

#include "DOMSVGTransformList.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsID.h"
#include "nsSVGTransform.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"

class nsSVGElement;

struct gfxMatrix;

#define MOZ_SVG_LIST_INDEX_BIT_COUNT 31 // supports > 2 billion list items

namespace mozilla {
namespace dom {

class SVGMatrix;

/**
 * DOM wrapper for an SVG transform. See DOMSVGLength.h.
 */
class SVGTransform MOZ_FINAL : public nsWrapperCache
{
  friend class AutoChangeTransformNotifier;

public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(SVGTransform)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(SVGTransform)

  /**
   * Generic ctor for SVGTransform objects that are created for an attribute.
   */
  SVGTransform(DOMSVGTransformList *aList,
               uint32_t aListIndex,
               bool aIsAnimValItem);

  /**
   * Ctors for creating the objects returned by:
   *   SVGSVGElement.createSVGTransform(),
   *   SVGSVGElement.createSVGTransformFromMatrix(in SVGMatrix matrix),
   *   SVGTransformList.createSVGTransformFromMatrix(in SVGMatrix matrix)
   * which do not initially belong to an attribute.
   */
  explicit SVGTransform();
  explicit SVGTransform(const gfxMatrix &aMatrix);

  /**
   * Ctor for creating an unowned copy. Used with Clone().
   */
  explicit SVGTransform(const nsSVGTransform &aMatrix);

  /**
   * Create an unowned copy of an owned transform. The caller is responsible for
   * the first AddRef().
   */
  SVGTransform* Clone() {
    NS_ASSERTION(mList, "unexpected caller");
    return new SVGTransform(InternalItem());
  }

  bool IsInList() const {
    return !!mList;
  }

  /**
   * In future, if this class is used for non-list transforms, this will be
   * different to IsInList().
   */
  bool HasOwner() const {
    return !!mList;
  }

  /**
   * This method is called to notify this DOM object that it is being inserted
   * into a list, and give it the information it needs as a result.
   *
   * This object MUST NOT already belong to a list when this method is called.
   * That's not to say that script can't move these DOM objects between
   * lists - it can - it's just that the logic to handle that (and send out
   * the necessary notifications) is located elsewhere (in
   * DOMSVGTransformList).)
   */
  void InsertingIntoList(DOMSVGTransformList *aList,
                         uint32_t aListIndex,
                         bool aIsAnimValItem);

  static uint32_t MaxListIndex() {
    return (1U << MOZ_SVG_LIST_INDEX_BIT_COUNT) - 1;
  }

  /// This method is called to notify this object that its list index changed.
  void UpdateListIndex(uint32_t aListIndex) {
    mListIndex = aListIndex;
  }

  /**
   * This method is called to notify this DOM object that it is about to be
   * removed from its current DOM list so that it can first make a copy of its
   * internal counterpart's values. (If it didn't do this, then it would
   * "lose" its value on being removed.)
   */
  void RemovingFromList();

  nsSVGTransform ToSVGTransform() const {
    return Transform();
  }

  // WebIDL
  DOMSVGTransformList* GetParentObject() const { return mList; }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;
  uint16_t Type() const;
  dom::SVGMatrix* GetMatrix();
  float Angle() const;
  void SetMatrix(dom::SVGMatrix& matrix, ErrorResult& rv);
  void SetTranslate(float tx, float ty, ErrorResult& rv);
  void SetScale(float sx, float sy, ErrorResult& rv);
  void SetRotate(float angle, float cx, float cy, ErrorResult& rv);
  void SetSkewX(float angle, ErrorResult& rv);
  void SetSkewY(float angle, ErrorResult& rv);

protected:
  ~SVGTransform();

  // Interface for SVGMatrix's use
  friend class dom::SVGMatrix;
  const bool IsAnimVal() const {
    return mIsAnimValItem;
  }
  const gfxMatrix& Matrixgfx() const {
    return Transform().GetMatrix();
  }
  void SetMatrix(const gfxMatrix& aMatrix);

private:
  nsSVGElement* Element() {
    return mList->Element();
  }

  /**
   * Get a reference to the internal nsSVGTransform list item that this DOM
   * wrapper object currently wraps.
   */
  nsSVGTransform& InternalItem();
  const nsSVGTransform& InternalItem() const;

#ifdef DEBUG
  bool IndexIsValid();
#endif

  const nsSVGTransform& Transform() const {
    return HasOwner() ? InternalItem() : *mTransform;
  }
  nsSVGTransform& Transform() {
    return HasOwner() ? InternalItem() : *mTransform;
  }

  nsRefPtr<DOMSVGTransformList> mList;

  // Bounds for the following are checked in the ctor, so be sure to update
  // that if you change the capacity of any of the following.

  uint32_t mListIndex:MOZ_SVG_LIST_INDEX_BIT_COUNT;
  uint32_t mIsAnimValItem:1;

  // Usually this class acts as a wrapper for an nsSVGTransform object which is
  // part of a list and is accessed by going via the owning Element.
  //
  // However, in some circumstances, objects of this class may not be associated
  // with any particular list and thus, no internal nsSVGTransform object. In
  // that case we allocate an nsSVGTransform object on the heap to store the data.
  nsAutoPtr<nsSVGTransform> mTransform;
};

} // namespace dom
} // namespace mozilla

#undef MOZ_SVG_LIST_INDEX_BIT_COUNT

#endif // mozilla_dom_SVGTransform_h
