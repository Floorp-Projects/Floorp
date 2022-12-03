/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_DOMSVGTRANSFORM_H_
#define DOM_SVG_DOMSVGTRANSFORM_H_

#include "DOMSVGTransformList.h"
#include "gfxMatrix.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsID.h"
#include "SVGTransform.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"

#define MOZ_SVG_LIST_INDEX_BIT_COUNT 31  // supports > 2 billion list items

namespace mozilla::dom {

struct DOMMatrix2DInit;
class SVGElement;
class SVGMatrix;

/**
 * DOM wrapper for an SVG transform. See DOMSVGLength.h.
 */
class DOMSVGTransform final : public nsWrapperCache {
  template <class T>
  friend class AutoChangeTransformListNotifier;

 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DOMSVGTransform)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DOMSVGTransform)

  /**
   * Generic ctor for DOMSVGTransform objects that are created for an attribute.
   */
  DOMSVGTransform(DOMSVGTransformList* aList, uint32_t aListIndex,
                  bool aIsAnimValItem);

  /**
   * Ctors for creating the objects returned by:
   *   SVGSVGElement.createSVGTransform(),
   *   SVGSVGElement.createSVGTransformFromMatrix(in DOMMatrix2DInit  matrix),
   *   SVGTransformList.createSVGTransformFromMatrix(in DOMMatrix2DInit  matrix)
   * which do not initially belong to an attribute.
   */
  DOMSVGTransform();
  explicit DOMSVGTransform(const gfxMatrix& aMatrix);
  DOMSVGTransform(const DOMMatrix2DInit& aMatrix, ErrorResult& rv);

  /**
   * Ctor for creating an unowned copy. Used with Clone().
   */
  explicit DOMSVGTransform(const SVGTransform& aTransform);

  /**
   * Create an unowned copy of an owned transform. The caller is responsible for
   * the first AddRef().
   */
  DOMSVGTransform* Clone() {
    NS_ASSERTION(mList, "unexpected caller");
    return new DOMSVGTransform(InternalItem());
  }

  bool IsInList() const { return !!mList; }

  /**
   * Returns true if our attribute is animating (in which case our animVal is
   * not simply a mirror of our baseVal).
   */
  bool IsAnimating() const { return mList && mList->IsAnimating(); }

  /**
   * In future, if this class is used for non-list transforms, this will be
   * different to IsInList().
   */
  bool HasOwner() const { return !!mList; }

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
  void InsertingIntoList(DOMSVGTransformList* aList, uint32_t aListIndex,
                         bool aIsAnimValItem);

  static uint32_t MaxListIndex() {
    return (1U << MOZ_SVG_LIST_INDEX_BIT_COUNT) - 1;
  }

  /// This method is called to notify this object that its list index changed.
  void UpdateListIndex(uint32_t aListIndex) { mListIndex = aListIndex; }

  /**
   * This method is called to notify this DOM object that it is about to be
   * removed from its current DOM list so that it can first make a copy of its
   * internal counterpart's values. (If it didn't do this, then it would
   * "lose" its value on being removed.)
   */
  void RemovingFromList();

  SVGTransform ToSVGTransform() const { return Transform(); }

  // WebIDL
  DOMSVGTransformList* GetParentObject() const { return mList; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  uint16_t Type() const;
  dom::SVGMatrix* GetMatrix();
  float Angle() const;
  void SetMatrix(const DOMMatrix2DInit& matrix, ErrorResult& rv);
  void SetTranslate(float tx, float ty, ErrorResult& rv);
  void SetScale(float sx, float sy, ErrorResult& rv);
  void SetRotate(float angle, float cx, float cy, ErrorResult& rv);
  void SetSkewX(float angle, ErrorResult& rv);
  void SetSkewY(float angle, ErrorResult& rv);

 protected:
  ~DOMSVGTransform();

  // Interface for SVGMatrix's use
  friend class dom::SVGMatrix;
  bool IsAnimVal() const { return mIsAnimValItem; }
  const gfxMatrix& Matrixgfx() const { return Transform().GetMatrix(); }
  void SetMatrix(const gfxMatrix& aMatrix);

 private:
  SVGElement* Element() { return mList->Element(); }

  /**
   * Get a reference to the internal SVGTransform list item that this DOM
   * wrapper object currently wraps.
   */
  SVGTransform& InternalItem();
  const SVGTransform& InternalItem() const;

#ifdef DEBUG
  bool IndexIsValid();
#endif

  const SVGTransform& Transform() const {
    return HasOwner() ? InternalItem() : *mTransform;
  }
  SVGTransform& Transform() {
    return HasOwner() ? InternalItem() : *mTransform;
  }

  RefPtr<DOMSVGTransformList> mList;

  // Bounds for the following are checked in the ctor, so be sure to update
  // that if you change the capacity of any of the following.

  uint32_t mListIndex : MOZ_SVG_LIST_INDEX_BIT_COUNT;
  uint32_t mIsAnimValItem : 1;

  // Usually this class acts as a wrapper for an SVGTransform object which is
  // part of a list and is accessed by going via the owning Element.
  //
  // However, in some circumstances, objects of this class may not be associated
  // with any particular list and thus, no internal SVGTransform object. In
  // that case we allocate an SVGTransform object on the heap to store the
  // data.
  UniquePtr<SVGTransform> mTransform;
};

}  // namespace mozilla::dom

#undef MOZ_SVG_LIST_INDEX_BIT_COUNT

#endif  // DOM_SVG_DOMSVGTRANSFORM_H_
