/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_DOMSVGPATHSEG_H_
#define DOM_SVG_DOMSVGPATHSEG_H_

#include "DOMSVGPathSegList.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "SVGPathSegUtils.h"
#include "mozilla/dom/SVGPathSegBinding.h"

#define MOZ_SVG_LIST_INDEX_BIT_COUNT 31

namespace mozilla {
namespace dom {

class SVGElement;

#define CHECK_ARG_COUNT_IN_SYNC(segType)                                   \
  MOZ_ASSERT(                                                              \
      ArrayLength(mArgs) ==                                                \
              SVGPathSegUtils::ArgCountForType(uint32_t(segType)) ||       \
          uint32_t(segType) == dom::SVGPathSeg_Binding::PATHSEG_CLOSEPATH, \
      "Arg count/array size out of sync")

#define IMPL_SVGPATHSEG_SUBCLASS_COMMON(segName, segType)                     \
  explicit DOMSVGPathSeg##segName(const float* aArgs) : DOMSVGPathSeg() {     \
    CHECK_ARG_COUNT_IN_SYNC(segType);                                         \
    memcpy(                                                                   \
        mArgs, aArgs,                                                         \
        SVGPathSegUtils::ArgCountForType(uint32_t(segType)) * sizeof(float)); \
  }                                                                           \
  DOMSVGPathSeg##segName(DOMSVGPathSegList* aList, uint32_t aListIndex,       \
                         bool aIsAnimValItem)                                 \
      : DOMSVGPathSeg(aList, aListIndex, aIsAnimValItem) {                    \
    CHECK_ARG_COUNT_IN_SYNC(segType);                                         \
  }                                                                           \
  /* From DOMSVGPathSeg: */                                                   \
  virtual uint32_t Type() const override { return segType; }                  \
  virtual DOMSVGPathSeg* Clone() override {                                   \
    /* InternalItem() + 1, because we're skipping the encoded seg type */     \
    float* args = IsInList() ? InternalItem() + 1 : mArgs;                    \
    return new DOMSVGPathSeg##segName(args);                                  \
  }                                                                           \
  virtual float* PtrToMemberArgs() override { return mArgs; }                 \
                                                                              \
  virtual JSObject* WrapObject(JSContext* aCx,                                \
                               JS::Handle<JSObject*> aGivenProto) override {  \
    return dom::SVGPathSeg##segName##_Binding::Wrap(aCx, this, aGivenProto);  \
  }

/**
 * Class DOMSVGPathSeg
 *
 * This class is the base class of the classes that create the DOM objects that
 * wrap the internal path segments that are encoded in an SVGPathData. Its
 * sub-classes are also used to create the objects returned by
 * SVGPathElement.createSVGPathSegXxx().
 *
 * See the architecture comment in DOMSVGPathSegList.h for an overview of the
 * important points regarding these DOM wrapper structures.
 *
 * See the architecture comment in DOMSVGLength.h (yes, LENGTH) for an overview
 * of the important points regarding how this specific class works.
 *
 * The main differences between this class and DOMSVGLength is that we have
 * sub-classes (it does not), and the "internal counterpart" that we provide a
 * DOM wrapper for is a list of floats, not an instance of an internal class.
 */
class DOMSVGPathSeg : public nsWrapperCache {
  template <class T>
  friend class AutoChangePathSegListNotifier;

 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(DOMSVGPathSeg)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(DOMSVGPathSeg)

  /**
   * Unlike the other list classes, we hide our ctor (because no one should be
   * creating instances of this class directly). This factory method in exposed
   * instead to take care of creating instances of the correct sub-class.
   */
  static DOMSVGPathSeg* CreateFor(DOMSVGPathSegList* aList, uint32_t aListIndex,
                                  bool aIsAnimValItem);

  /**
   * Create an unowned copy of this object. The caller is responsible for the
   * first AddRef()!
   */
  virtual DOMSVGPathSeg* Clone() = 0;

  bool IsInList() const { return !!mList; }

  /**
   * Returns true if our attribute is animating (in which case our animVal is
   * not simply a mirror of our baseVal).
   */
  bool AttrIsAnimating() const { return mList && mList->AttrIsAnimating(); }

  /**
   * In future, if this class is used for non-list segments, this will be
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
   * the necessary notifications) is located elsewhere (in DOMSVGPathSegList).)
   */
  void InsertingIntoList(DOMSVGPathSegList* aList, uint32_t aListIndex,
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

  /**
   * This method converts the segment to a string of floats as found in
   * SVGPathData (i.e. the first float contains the type of the segment,
   * encoded into a float, followed by its arguments in the same order as they
   * are given in the <path> element's 'd' attribute).
   */
  void ToSVGPathSegEncodedData(float* aRaw);

  /**
   * The type of this path segment.
   */
  virtual uint32_t Type() const = 0;

  // WebIDL
  DOMSVGPathSegList* GetParentObject() { return mList; }
  uint16_t PathSegType() const { return Type(); }
  void GetPathSegTypeAsLetter(nsAString& aPathSegTypeAsLetter) {
    aPathSegTypeAsLetter = SVGPathSegUtils::GetPathSegTypeAsLetter(Type());
  }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override = 0;

 protected:
  /**
   * Generic ctor for DOMSVGPathSeg objects that are created for an attribute.
   */
  DOMSVGPathSeg(DOMSVGPathSegList* aList, uint32_t aListIndex,
                bool aIsAnimValItem);

  /**
   * Ctor for creating the objects returned by
   * SVGPathElement.createSVGPathSegXxx(), which do not initially belong to an
   * attribute.
   */
  DOMSVGPathSeg();

  virtual ~DOMSVGPathSeg() {
    // Our mList's weak ref to us must be nulled out when we die. If GC has
    // unlinked us using the cycle collector code, then that has already
    // happened, and mList is null.
    if (mList) {
      mList->ItemAt(mListIndex) = nullptr;
    }
  }

  dom::SVGElement* Element() { return mList->Element(); }

  /**
   * Get a reference to the internal SVGPathSeg list item that this DOM wrapper
   * object currently wraps.
   *
   * To simplify the code we just have this one method for obtaining both
   * baseVal and animVal internal items. This means that animVal items don't
   * get const protection, but then our setter methods guard against changing
   * animVal items.
   */
  float* InternalItem();

  virtual float* PtrToMemberArgs() = 0;

#ifdef DEBUG
  bool IndexIsValid();
#endif

  RefPtr<DOMSVGPathSegList> mList;

  // Bounds for the following are checked in the ctor, so be sure to update
  // that if you change the capacity of any of the following.

  uint32_t mListIndex : MOZ_SVG_LIST_INDEX_BIT_COUNT;
  uint32_t mIsAnimValItem : 1;  // uint32_t because MSVC won't pack otherwise
};

class DOMSVGPathSegClosePath : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegClosePath() : DOMSVGPathSeg() {}

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(ClosePath,
                                  dom::SVGPathSeg_Binding::PATHSEG_CLOSEPATH)

 protected:
  // To allow IMPL_SVGPATHSEG_SUBCLASS_COMMON above to compile we need an
  // mArgs, but since C++ doesn't allow zero-sized arrays we need to give it
  // one (unused) element.
  float mArgs[1];
};

class DOMSVGPathSegMovetoAbs : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegMovetoAbs(float x, float y) : DOMSVGPathSeg() {
    mArgs[0] = x;
    mArgs[1] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(MovetoAbs,
                                  dom::SVGPathSeg_Binding::PATHSEG_MOVETO_ABS)

  float X();
  void SetX(float aX, ErrorResult& rv);
  float Y();
  void SetY(float aY, ErrorResult& rv);

 protected:
  float mArgs[2];
};

class DOMSVGPathSegMovetoRel : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegMovetoRel(float x, float y) : DOMSVGPathSeg() {
    mArgs[0] = x;
    mArgs[1] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(MovetoRel,
                                  dom::SVGPathSeg_Binding::PATHSEG_MOVETO_REL)

  float X();
  void SetX(float aX, ErrorResult& rv);
  float Y();
  void SetY(float aY, ErrorResult& rv);

 protected:
  float mArgs[2];
};

class DOMSVGPathSegLinetoAbs : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegLinetoAbs(float x, float y) : DOMSVGPathSeg() {
    mArgs[0] = x;
    mArgs[1] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(LinetoAbs,
                                  dom::SVGPathSeg_Binding::PATHSEG_LINETO_ABS)

  float X();
  void SetX(float aX, ErrorResult& rv);
  float Y();
  void SetY(float aY, ErrorResult& rv);

 protected:
  float mArgs[2];
};

class DOMSVGPathSegLinetoRel : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegLinetoRel(float x, float y) : DOMSVGPathSeg() {
    mArgs[0] = x;
    mArgs[1] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(LinetoRel,
                                  dom::SVGPathSeg_Binding::PATHSEG_LINETO_REL)

  float X();
  void SetX(float aX, ErrorResult& rv);
  float Y();
  void SetY(float aY, ErrorResult& rv);

 protected:
  float mArgs[2];
};

class DOMSVGPathSegCurvetoCubicAbs : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegCurvetoCubicAbs(float x1, float y1, float x2, float y2, float x,
                               float y)
      : DOMSVGPathSeg() {
    mArgs[0] = x1;
    mArgs[1] = y1;
    mArgs[2] = x2;
    mArgs[3] = y2;
    mArgs[4] = x;
    mArgs[5] = y;
  }

  float X();
  void SetX(float aX, ErrorResult& rv);
  float Y();
  void SetY(float aY, ErrorResult& rv);
  float X1();
  void SetX1(float aX1, ErrorResult& rv);
  float Y1();
  void SetY1(float aY1, ErrorResult& rv);
  float X2();
  void SetX2(float aX2, ErrorResult& rv);
  float Y2();
  void SetY2(float aY2, ErrorResult& rv);

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(
      CurvetoCubicAbs, dom::SVGPathSeg_Binding::PATHSEG_CURVETO_CUBIC_ABS)

 protected:
  float mArgs[6];
};

class DOMSVGPathSegCurvetoCubicRel : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegCurvetoCubicRel(float x1, float y1, float x2, float y2, float x,
                               float y)
      : DOMSVGPathSeg() {
    mArgs[0] = x1;
    mArgs[1] = y1;
    mArgs[2] = x2;
    mArgs[3] = y2;
    mArgs[4] = x;
    mArgs[5] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(
      CurvetoCubicRel, dom::SVGPathSeg_Binding::PATHSEG_CURVETO_CUBIC_REL)

  float X();
  void SetX(float aX, ErrorResult& rv);
  float Y();
  void SetY(float aY, ErrorResult& rv);
  float X1();
  void SetX1(float aX1, ErrorResult& rv);
  float Y1();
  void SetY1(float aY1, ErrorResult& rv);
  float X2();
  void SetX2(float aX2, ErrorResult& rv);
  float Y2();
  void SetY2(float aY2, ErrorResult& rv);

 protected:
  float mArgs[6];
};

class DOMSVGPathSegCurvetoQuadraticAbs : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegCurvetoQuadraticAbs(float x1, float y1, float x, float y)
      : DOMSVGPathSeg() {
    mArgs[0] = x1;
    mArgs[1] = y1;
    mArgs[2] = x;
    mArgs[3] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(
      CurvetoQuadraticAbs,
      dom::SVGPathSeg_Binding::PATHSEG_CURVETO_QUADRATIC_ABS)

  float X();
  void SetX(float aX, ErrorResult& rv);
  float Y();
  void SetY(float aY, ErrorResult& rv);
  float X1();
  void SetX1(float aX1, ErrorResult& rv);
  float Y1();
  void SetY1(float aY1, ErrorResult& rv);

 protected:
  float mArgs[4];
};

class DOMSVGPathSegCurvetoQuadraticRel : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegCurvetoQuadraticRel(float x1, float y1, float x, float y)
      : DOMSVGPathSeg() {
    mArgs[0] = x1;
    mArgs[1] = y1;
    mArgs[2] = x;
    mArgs[3] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(
      CurvetoQuadraticRel,
      dom::SVGPathSeg_Binding::PATHSEG_CURVETO_QUADRATIC_REL)

  float X();
  void SetX(float aX, ErrorResult& rv);
  float Y();
  void SetY(float aY, ErrorResult& rv);
  float X1();
  void SetX1(float aX1, ErrorResult& rv);
  float Y1();
  void SetY1(float aY1, ErrorResult& rv);

 protected:
  float mArgs[4];
};

class DOMSVGPathSegArcAbs : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegArcAbs(float r1, float r2, float angle, bool largeArcFlag,
                      bool sweepFlag, float x, float y)
      : DOMSVGPathSeg() {
    mArgs[0] = r1;
    mArgs[1] = r2;
    mArgs[2] = angle;
    mArgs[3] = largeArcFlag;
    mArgs[4] = sweepFlag;
    mArgs[5] = x;
    mArgs[6] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(ArcAbs,
                                  dom::SVGPathSeg_Binding::PATHSEG_ARC_ABS)

  float X();
  void SetX(float aX, ErrorResult& rv);
  float Y();
  void SetY(float aY, ErrorResult& rv);
  float R1();
  void SetR1(float aR1, ErrorResult& rv);
  float R2();
  void SetR2(float aR2, ErrorResult& rv);
  float Angle();
  void SetAngle(float aAngle, ErrorResult& rv);
  bool LargeArcFlag();
  void SetLargeArcFlag(bool aLargeArcFlag, ErrorResult& rv);
  bool SweepFlag();
  void SetSweepFlag(bool aSweepFlag, ErrorResult& rv);

 protected:
  float mArgs[7];
};

class DOMSVGPathSegArcRel : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegArcRel(float r1, float r2, float angle, bool largeArcFlag,
                      bool sweepFlag, float x, float y)
      : DOMSVGPathSeg() {
    mArgs[0] = r1;
    mArgs[1] = r2;
    mArgs[2] = angle;
    mArgs[3] = largeArcFlag;
    mArgs[4] = sweepFlag;
    mArgs[5] = x;
    mArgs[6] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(ArcRel,
                                  dom::SVGPathSeg_Binding::PATHSEG_ARC_REL)

  float X();
  void SetX(float aX, ErrorResult& rv);
  float Y();
  void SetY(float aY, ErrorResult& rv);
  float R1();
  void SetR1(float aR1, ErrorResult& rv);
  float R2();
  void SetR2(float aR2, ErrorResult& rv);
  float Angle();
  void SetAngle(float aAngle, ErrorResult& rv);
  bool LargeArcFlag();
  void SetLargeArcFlag(bool aLargeArcFlag, ErrorResult& rv);
  bool SweepFlag();
  void SetSweepFlag(bool aSweepFlag, ErrorResult& rv);

 protected:
  float mArgs[7];
};

class DOMSVGPathSegLinetoHorizontalAbs : public DOMSVGPathSeg {
 public:
  explicit DOMSVGPathSegLinetoHorizontalAbs(float x) : DOMSVGPathSeg() {
    mArgs[0] = x;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(
      LinetoHorizontalAbs,
      dom::SVGPathSeg_Binding::PATHSEG_LINETO_HORIZONTAL_ABS)

  float X();
  void SetX(float aX, ErrorResult& rv);

 protected:
  float mArgs[1];
};

class DOMSVGPathSegLinetoHorizontalRel : public DOMSVGPathSeg {
 public:
  explicit DOMSVGPathSegLinetoHorizontalRel(float x) : DOMSVGPathSeg() {
    mArgs[0] = x;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(
      LinetoHorizontalRel,
      dom::SVGPathSeg_Binding::PATHSEG_LINETO_HORIZONTAL_REL)

  float X();
  void SetX(float aX, ErrorResult& rv);

 protected:
  float mArgs[1];
};

class DOMSVGPathSegLinetoVerticalAbs : public DOMSVGPathSeg {
 public:
  explicit DOMSVGPathSegLinetoVerticalAbs(float y) : DOMSVGPathSeg() {
    mArgs[0] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(
      LinetoVerticalAbs, dom::SVGPathSeg_Binding::PATHSEG_LINETO_VERTICAL_ABS)

  float Y();
  void SetY(float aY, ErrorResult& rv);

 protected:
  float mArgs[1];
};

class DOMSVGPathSegLinetoVerticalRel : public DOMSVGPathSeg {
 public:
  explicit DOMSVGPathSegLinetoVerticalRel(float y) : DOMSVGPathSeg() {
    mArgs[0] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(
      LinetoVerticalRel, dom::SVGPathSeg_Binding::PATHSEG_LINETO_VERTICAL_REL)

  float Y();
  void SetY(float aY, ErrorResult& rv);

 protected:
  float mArgs[1];
};

class DOMSVGPathSegCurvetoCubicSmoothAbs : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegCurvetoCubicSmoothAbs(float x2, float y2, float x, float y)
      : DOMSVGPathSeg() {
    mArgs[0] = x2;
    mArgs[1] = y2;
    mArgs[2] = x;
    mArgs[3] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(
      CurvetoCubicSmoothAbs,
      dom::SVGPathSeg_Binding::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS)

  float X();
  void SetX(float aX, ErrorResult& rv);
  float Y();
  void SetY(float aY, ErrorResult& rv);
  float X2();
  void SetX2(float aX2, ErrorResult& rv);
  float Y2();
  void SetY2(float aY2, ErrorResult& rv);

 protected:
  float mArgs[4];
};

class DOMSVGPathSegCurvetoCubicSmoothRel : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegCurvetoCubicSmoothRel(float x2, float y2, float x, float y)
      : DOMSVGPathSeg() {
    mArgs[0] = x2;
    mArgs[1] = y2;
    mArgs[2] = x;
    mArgs[3] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(
      CurvetoCubicSmoothRel,
      dom::SVGPathSeg_Binding::PATHSEG_CURVETO_CUBIC_SMOOTH_REL)

  float X();
  void SetX(float aX, ErrorResult& rv);
  float Y();
  void SetY(float aY, ErrorResult& rv);
  float X2();
  void SetX2(float aX2, ErrorResult& rv);
  float Y2();
  void SetY2(float aY2, ErrorResult& rv);

 protected:
  float mArgs[4];
};

class DOMSVGPathSegCurvetoQuadraticSmoothAbs : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y) : DOMSVGPathSeg() {
    mArgs[0] = x;
    mArgs[1] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(
      CurvetoQuadraticSmoothAbs,
      dom::SVGPathSeg_Binding::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS)

  float X();
  void SetX(float aX, ErrorResult& rv);
  float Y();
  void SetY(float aY, ErrorResult& rv);

 protected:
  float mArgs[2];
};

class DOMSVGPathSegCurvetoQuadraticSmoothRel : public DOMSVGPathSeg {
 public:
  DOMSVGPathSegCurvetoQuadraticSmoothRel(float x, float y) : DOMSVGPathSeg() {
    mArgs[0] = x;
    mArgs[1] = y;
  }

  IMPL_SVGPATHSEG_SUBCLASS_COMMON(
      CurvetoQuadraticSmoothRel,
      dom::SVGPathSeg_Binding::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL)

  float X();
  void SetX(float aX, ErrorResult& rv);
  float Y();
  void SetY(float aY, ErrorResult& rv);

 protected:
  float mArgs[2];
};

}  // namespace dom
}  // namespace mozilla

#undef MOZ_SVG_LIST_INDEX_BIT_COUNT

#endif  // DOM_SVG_DOMSVGPATHSEG_H_
