/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "DOMSVGPathSeg.h"
#include "DOMSVGPathSegList.h"
#include "SVGPathSegUtils.h"
#include "SVGAnimatedPathSegList.h"
#include "nsSVGElement.h"
#include "nsIDOMSVGPathSeg.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"

// See the architecture comment in DOMSVGPathSegList.h.

using namespace mozilla;

// We could use NS_IMPL_CYCLE_COLLECTION_1, except that in Unlink() we need to
// clear our list's weak ref to us to be safe. (The other option would be to
// not unlink and rely on the breaking of the other edges in the cycle, as
// NS_SVG_VAL_IMPL_CYCLE_COLLECTION does.)
NS_IMPL_CYCLE_COLLECTION_CLASS(DOMSVGPathSeg)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGPathSeg)
  // We may not belong to a list, so we must null check tmp->mList.
  if (tmp->mList) {
    tmp->mList->ItemAt(tmp->mListIndex) = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGPathSeg)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGPathSeg)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGPathSeg)

DOMCI_DATA(SVGPathSeg, DOMSVGPathSeg)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGPathSeg)
  NS_INTERFACE_MAP_ENTRY(DOMSVGPathSeg) // pseudo-interface
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPathSeg)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


DOMSVGPathSeg::DOMSVGPathSeg(DOMSVGPathSegList *aList,
                             PRUint32 aListIndex,
                             bool aIsAnimValItem)
  : mList(aList)
  , mListIndex(aListIndex)
  , mIsAnimValItem(aIsAnimValItem)
{
  // These shifts are in sync with the members in the header.
  NS_ABORT_IF_FALSE(aList &&
                    aListIndex <= MaxListIndex(), "bad arg");

  NS_ABORT_IF_FALSE(IndexIsValid(), "Bad index for DOMSVGPathSeg!");
}

DOMSVGPathSeg::DOMSVGPathSeg()
  : mList(nullptr)
  , mListIndex(0)
  , mIsAnimValItem(false)
{
}

NS_IMETHODIMP
DOMSVGPathSeg::GetPathSegType(PRUint16 *aPathSegType)
{
  *aPathSegType = PRUint16(Type());
  return NS_OK;
}

NS_IMETHODIMP
DOMSVGPathSeg::GetPathSegTypeAsLetter(nsAString &aPathSegTypeAsLetter)
{
  aPathSegTypeAsLetter = SVGPathSegUtils::GetPathSegTypeAsLetter(Type());
  return NS_OK;
}

void
DOMSVGPathSeg::InsertingIntoList(DOMSVGPathSegList *aList,
                                 PRUint32 aListIndex,
                                 bool aIsAnimValItem)
{
  NS_ABORT_IF_FALSE(!HasOwner(), "Inserting item that is already in a list");

  mList = aList;
  mListIndex = aListIndex;
  mIsAnimValItem = aIsAnimValItem;

  NS_ABORT_IF_FALSE(IndexIsValid(), "Bad index for DOMSVGPathSeg!");
}

void
DOMSVGPathSeg::RemovingFromList()
{
  PRUint32 argCount = SVGPathSegUtils::ArgCountForType(Type());
  // InternalItem() + 1, because the args come after the encoded seg type
  memcpy(PtrToMemberArgs(), InternalItem() + 1, argCount * sizeof(float));
  mList = nullptr;
  mIsAnimValItem = false;
}

void
DOMSVGPathSeg::ToSVGPathSegEncodedData(float* aRaw)
{
  NS_ABORT_IF_FALSE(aRaw, "null pointer");
  PRUint32 argCount = SVGPathSegUtils::ArgCountForType(Type());
  if (IsInList()) {
    // 1 + argCount, because we're copying the encoded seg type and args
    memcpy(aRaw, InternalItem(), (1 + argCount) * sizeof(float));
  } else {
    aRaw[0] = SVGPathSegUtils::EncodeType(Type());
    // aRaw + 1, because the args go after the encoded seg type
    memcpy(aRaw + 1, PtrToMemberArgs(), argCount * sizeof(float));
  }
}

float*
DOMSVGPathSeg::InternalItem()
{
  PRUint32 dataIndex = mList->mItems[mListIndex].mInternalDataIndex;
  return &(mList->InternalList().mData[dataIndex]);
}

#ifdef DEBUG
bool
DOMSVGPathSeg::IndexIsValid()
{
  SVGAnimatedPathSegList *alist = Element()->GetAnimPathSegList();
  return (mIsAnimValItem &&
          mListIndex < alist->GetAnimValue().CountItems()) ||
         (!mIsAnimValItem &&
          mListIndex < alist->GetBaseValue().CountItems());
}
#endif


////////////////////////////////////////////////////////////////////////
// Implementation of DOMSVGPathSeg sub-classes below this point

#define CHECK_ARG_COUNT_IN_SYNC(segType)                                      \
          NS_ABORT_IF_FALSE(ArrayLength(mArgs) ==                         \
            SVGPathSegUtils::ArgCountForType(PRUint32(segType)) ||            \
            PRUint32(segType) == nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH,         \
            "Arg count/array size out of sync")

#define IMPL_SVGPATHSEG_SUBCLASS_COMMON(segName, segType)                     \
  DOMSVGPathSeg##segName(const float *aArgs)                                  \
    : DOMSVGPathSeg()                                                         \
  {                                                                           \
    CHECK_ARG_COUNT_IN_SYNC(segType);                                         \
    memcpy(mArgs, aArgs,                                                      \
        SVGPathSegUtils::ArgCountForType(PRUint32(segType)) * sizeof(float)); \
  }                                                                           \
  DOMSVGPathSeg##segName(DOMSVGPathSegList *aList,                            \
                         PRUint32 aListIndex,                                 \
                         bool aIsAnimValItem)                               \
    : DOMSVGPathSeg(aList, aListIndex, aIsAnimValItem)                        \
  {                                                                           \
    CHECK_ARG_COUNT_IN_SYNC(segType);                                         \
  }                                                                           \
  /* From DOMSVGPathSeg: */                                                   \
  virtual PRUint32                                                            \
  Type() const                                                                \
  {                                                                           \
    return segType;                                                           \
  }                                                                           \
  virtual DOMSVGPathSeg*                                                      \
  Clone()                                                                     \
  {                                                                           \
    /* InternalItem() + 1, because we're skipping the encoded seg type */     \
    float *args = IsInList() ? InternalItem() + 1 : mArgs;                    \
    return new DOMSVGPathSeg##segName(args);                                  \
  }                                                                           \
  virtual float*                                                              \
  PtrToMemberArgs()                                                           \
  {                                                                           \
    return mArgs;                                                             \
  }

#define IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(segName)                         \
  /* Forward to the CYCLE_COLLECTING_ADDREF on our base class */              \
  NS_IMPL_ADDREF_INHERITED(DOMSVGPathSeg##segName, DOMSVGPathSeg)             \
  NS_IMPL_RELEASE_INHERITED(DOMSVGPathSeg##segName, DOMSVGPathSeg)            \
                                                                              \
  DOMCI_DATA(SVGPathSeg##segName, DOMSVGPathSeg##segName)                     \
                                                                              \
  NS_INTERFACE_MAP_BEGIN(DOMSVGPathSeg##segName)                              \
    NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPathSeg##segName)                         \
    NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGPathSeg##segName)                 \
  NS_INTERFACE_MAP_END_INHERITING(DOMSVGPathSeg)

#define IMPL_PROP_WITH_TYPE(segName, propName, index, type)                   \
  /* attribute type propName; */                                              \
  NS_IMETHODIMP                                                               \
  DOMSVGPathSeg##segName::Get##propName(type *a##propName)                    \
  {                                                                           \
    if (mIsAnimValItem && HasOwner()) {                                       \
      Element()->FlushAnimations(); /* May make HasOwner() == false */     \
    }                                                                         \
    *a##propName = type(HasOwner() ? InternalItem()[1+index] : mArgs[index]); \
    return NS_OK;                                                             \
  }                                                                           \
  NS_IMETHODIMP                                                               \
  DOMSVGPathSeg##segName::Set##propName(type a##propName)                     \
  {                                                                           \
    if (mIsAnimValItem) {                                                     \
      return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;                        \
    }                                                                         \
    NS_ENSURE_FINITE(float(a##propName), NS_ERROR_ILLEGAL_VALUE);             \
    if (HasOwner()) {                                                         \
      if (InternalItem()[1+index] == float(a##propName)) {                    \
        return NS_OK;                                                         \
      }                                                                       \
      NS_ABORT_IF_FALSE(IsInList(), "Will/DidChangePathSegList() is wrong");  \
      nsAttrValue emptyOrOldValue = Element()->WillChangePathSegList();       \
      InternalItem()[1+index] = float(a##propName);                           \
      Element()->DidChangePathSegList(emptyOrOldValue);                       \
      if (mList->AttrIsAnimating()) {                                         \
        Element()->AnimationNeedsResample();                                  \
      }                                                                       \
    } else {                                                                  \
      mArgs[index] = float(a##propName);                                      \
    }                                                                         \
    return NS_OK;                                                             \
  }

// For float, the normal type of arguments
#define IMPL_FLOAT_PROP(segName, propName, index) \
  IMPL_PROP_WITH_TYPE(segName, propName, index, float)

// For the boolean flags in arc commands
#define IMPL_BOOL_PROP(segName, propName, index) \
  IMPL_PROP_WITH_TYPE(segName, propName, index, bool)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegClosePath
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegClosePath
{
public:
  DOMSVGPathSegClosePath()
    : DOMSVGPathSeg()
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGCLOSEPATH
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(ClosePath, PATHSEG_CLOSEPATH)

protected:
  // To allow IMPL_SVGPATHSEG_SUBCLASS_COMMON above to compile we need an
  // mArgs, but since C++ doesn't allow zero-sized arrays we need to give it
  // one (unused) element.
  float mArgs[1];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(ClosePath)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegMovetoAbs
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegMovetoAbs
{
public:
  DOMSVGPathSegMovetoAbs(float x, float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = x;
    mArgs[1] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGMOVETOABS
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(MovetoAbs, PATHSEG_MOVETO_ABS)

protected:
  float mArgs[2];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(MovetoAbs)

IMPL_FLOAT_PROP(MovetoAbs, X, 0)
IMPL_FLOAT_PROP(MovetoAbs, Y, 1)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegMovetoRel
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegMovetoRel
{
public:
  DOMSVGPathSegMovetoRel(float x, float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = x;
    mArgs[1] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGMOVETOREL
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(MovetoRel, PATHSEG_MOVETO_REL)

protected:
  float mArgs[2];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(MovetoRel)

IMPL_FLOAT_PROP(MovetoRel, X, 0)
IMPL_FLOAT_PROP(MovetoRel, Y, 1)



////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegLinetoAbs
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegLinetoAbs
{
public:
  DOMSVGPathSegLinetoAbs(float x, float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = x;
    mArgs[1] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGLINETOABS
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(LinetoAbs, PATHSEG_LINETO_ABS)

protected:
  float mArgs[2];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(LinetoAbs)

IMPL_FLOAT_PROP(LinetoAbs, X, 0)
IMPL_FLOAT_PROP(LinetoAbs, Y, 1)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegLinetoRel
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegLinetoRel
{
public:
  DOMSVGPathSegLinetoRel(float x, float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = x;
    mArgs[1] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGLINETOREL
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(LinetoRel, PATHSEG_LINETO_REL)

protected:
  float mArgs[2];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(LinetoRel)

IMPL_FLOAT_PROP(LinetoRel, X, 0)
IMPL_FLOAT_PROP(LinetoRel, Y, 1)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegCurvetoCubicAbs
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegCurvetoCubicAbs
{
public:
  DOMSVGPathSegCurvetoCubicAbs(float x1, float y1,
                               float x2, float y2,
                               float x, float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = x1;
    mArgs[1] = y1;
    mArgs[2] = x2;
    mArgs[3] = y2;
    mArgs[4] = x;
    mArgs[5] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGCURVETOCUBICABS
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(CurvetoCubicAbs, PATHSEG_CURVETO_CUBIC_ABS)

protected:
  float mArgs[6];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(CurvetoCubicAbs)

IMPL_FLOAT_PROP(CurvetoCubicAbs, X1, 0)
IMPL_FLOAT_PROP(CurvetoCubicAbs, Y1, 1)
IMPL_FLOAT_PROP(CurvetoCubicAbs, X2, 2)
IMPL_FLOAT_PROP(CurvetoCubicAbs, Y2, 3)
IMPL_FLOAT_PROP(CurvetoCubicAbs, X, 4)
IMPL_FLOAT_PROP(CurvetoCubicAbs, Y, 5)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegCurvetoCubicRel
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegCurvetoCubicRel
{
public:
  DOMSVGPathSegCurvetoCubicRel(float x1, float y1,
                               float x2, float y2,
                               float x, float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = x1;
    mArgs[1] = y1;
    mArgs[2] = x2;
    mArgs[3] = y2;
    mArgs[4] = x;
    mArgs[5] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGCURVETOCUBICREL
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(CurvetoCubicRel, PATHSEG_CURVETO_CUBIC_REL)

protected:
  float mArgs[6];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(CurvetoCubicRel)

IMPL_FLOAT_PROP(CurvetoCubicRel, X1, 0)
IMPL_FLOAT_PROP(CurvetoCubicRel, Y1, 1)
IMPL_FLOAT_PROP(CurvetoCubicRel, X2, 2)
IMPL_FLOAT_PROP(CurvetoCubicRel, Y2, 3)
IMPL_FLOAT_PROP(CurvetoCubicRel, X, 4)
IMPL_FLOAT_PROP(CurvetoCubicRel, Y, 5)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegCurvetoQuadraticAbs
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegCurvetoQuadraticAbs
{
public:
  DOMSVGPathSegCurvetoQuadraticAbs(float x1, float y1,
                                   float x, float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = x1;
    mArgs[1] = y1;
    mArgs[2] = x;
    mArgs[3] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGCURVETOQUADRATICABS
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(CurvetoQuadraticAbs, PATHSEG_CURVETO_QUADRATIC_ABS)

protected:
  float mArgs[4];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(CurvetoQuadraticAbs)

IMPL_FLOAT_PROP(CurvetoQuadraticAbs, X1, 0)
IMPL_FLOAT_PROP(CurvetoQuadraticAbs, Y1, 1)
IMPL_FLOAT_PROP(CurvetoQuadraticAbs, X, 2)
IMPL_FLOAT_PROP(CurvetoQuadraticAbs, Y, 3)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegCurvetoQuadraticRel
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegCurvetoQuadraticRel
{
public:
  DOMSVGPathSegCurvetoQuadraticRel(float x1, float y1,
                                   float x, float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = x1;
    mArgs[1] = y1;
    mArgs[2] = x;
    mArgs[3] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGCURVETOQUADRATICREL
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(CurvetoQuadraticRel, PATHSEG_CURVETO_QUADRATIC_REL)

protected:
  float mArgs[4];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(CurvetoQuadraticRel)

IMPL_FLOAT_PROP(CurvetoQuadraticRel, X1, 0)
IMPL_FLOAT_PROP(CurvetoQuadraticRel, Y1, 1)
IMPL_FLOAT_PROP(CurvetoQuadraticRel, X, 2)
IMPL_FLOAT_PROP(CurvetoQuadraticRel, Y, 3)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegArcAbs
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegArcAbs
{
public:
  DOMSVGPathSegArcAbs(float r1, float r2, float angle,
                      bool largeArcFlag, bool sweepFlag,
                      float x, float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = r1;
    mArgs[1] = r2;
    mArgs[2] = angle;
    mArgs[3] = largeArcFlag;
    mArgs[4] = sweepFlag;
    mArgs[5] = x;
    mArgs[6] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGARCABS
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(ArcAbs, PATHSEG_ARC_ABS)

protected:
  float mArgs[7];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(ArcAbs)

IMPL_FLOAT_PROP(ArcAbs, R1, 0)
IMPL_FLOAT_PROP(ArcAbs, R2, 1)
IMPL_FLOAT_PROP(ArcAbs, Angle, 2)
IMPL_BOOL_PROP(ArcAbs, LargeArcFlag, 3)
IMPL_BOOL_PROP(ArcAbs, SweepFlag, 4)
IMPL_FLOAT_PROP(ArcAbs, X, 5)
IMPL_FLOAT_PROP(ArcAbs, Y, 6)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegArcRel
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegArcRel
{
public:
  DOMSVGPathSegArcRel(float r1, float r2, float angle,
                      bool largeArcFlag, bool sweepFlag,
                      float x, float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = r1;
    mArgs[1] = r2;
    mArgs[2] = angle;
    mArgs[3] = largeArcFlag;
    mArgs[4] = sweepFlag;
    mArgs[5] = x;
    mArgs[6] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGARCREL
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(ArcRel, PATHSEG_ARC_REL)

protected:
  float mArgs[7];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(ArcRel)

IMPL_FLOAT_PROP(ArcRel, R1, 0)
IMPL_FLOAT_PROP(ArcRel, R2, 1)
IMPL_FLOAT_PROP(ArcRel, Angle, 2)
IMPL_BOOL_PROP(ArcRel, LargeArcFlag, 3)
IMPL_BOOL_PROP(ArcRel, SweepFlag, 4)
IMPL_FLOAT_PROP(ArcRel, X, 5)
IMPL_FLOAT_PROP(ArcRel, Y, 6)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegLinetoHorizontalAbs
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegLinetoHorizontalAbs
{
public:
  DOMSVGPathSegLinetoHorizontalAbs(float x)
    : DOMSVGPathSeg()
  {
    mArgs[0] = x;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGLINETOHORIZONTALABS
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(LinetoHorizontalAbs, PATHSEG_LINETO_HORIZONTAL_ABS)

protected:
  float mArgs[1];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(LinetoHorizontalAbs)

IMPL_FLOAT_PROP(LinetoHorizontalAbs, X, 0)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegLinetoHorizontalRel
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegLinetoHorizontalRel
{
public:
  DOMSVGPathSegLinetoHorizontalRel(float x)
    : DOMSVGPathSeg()
  {
    mArgs[0] = x;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGLINETOHORIZONTALREL
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(LinetoHorizontalRel, PATHSEG_LINETO_HORIZONTAL_REL)

protected:
  float mArgs[1];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(LinetoHorizontalRel)

IMPL_FLOAT_PROP(LinetoHorizontalRel, X, 0)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegLinetoVerticalAbs
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegLinetoVerticalAbs
{
public:
  DOMSVGPathSegLinetoVerticalAbs(float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGLINETOVERTICALABS
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(LinetoVerticalAbs, PATHSEG_LINETO_VERTICAL_ABS)

protected:
  float mArgs[1];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(LinetoVerticalAbs)

IMPL_FLOAT_PROP(LinetoVerticalAbs, Y, 0)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegLinetoVerticalRel
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegLinetoVerticalRel
{
public:
  DOMSVGPathSegLinetoVerticalRel(float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGLINETOVERTICALREL
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(LinetoVerticalRel, PATHSEG_LINETO_VERTICAL_REL)

protected:
  float mArgs[1];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(LinetoVerticalRel)

IMPL_FLOAT_PROP(LinetoVerticalRel, Y, 0)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegCurvetoCubicSmoothAbs
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegCurvetoCubicSmoothAbs
{
public:
  DOMSVGPathSegCurvetoCubicSmoothAbs(float x2, float y2,
                                     float x, float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = x2;
    mArgs[1] = y2;
    mArgs[2] = x;
    mArgs[3] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGCURVETOCUBICSMOOTHABS
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(CurvetoCubicSmoothAbs, PATHSEG_CURVETO_CUBIC_SMOOTH_ABS)

protected:
  float mArgs[4];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(CurvetoCubicSmoothAbs)

IMPL_FLOAT_PROP(CurvetoCubicSmoothAbs, X2, 0)
IMPL_FLOAT_PROP(CurvetoCubicSmoothAbs, Y2, 1)
IMPL_FLOAT_PROP(CurvetoCubicSmoothAbs, X, 2)
IMPL_FLOAT_PROP(CurvetoCubicSmoothAbs, Y, 3)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegCurvetoCubicSmoothRel
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegCurvetoCubicSmoothRel
{
public:
  DOMSVGPathSegCurvetoCubicSmoothRel(float x2, float y2,
                                     float x, float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = x2;
    mArgs[1] = y2;
    mArgs[2] = x;
    mArgs[3] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGCURVETOCUBICSMOOTHREL
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(CurvetoCubicSmoothRel, PATHSEG_CURVETO_CUBIC_SMOOTH_REL)

protected:
  float mArgs[4];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(CurvetoCubicSmoothRel)

IMPL_FLOAT_PROP(CurvetoCubicSmoothRel, X2, 0)
IMPL_FLOAT_PROP(CurvetoCubicSmoothRel, Y2, 1)
IMPL_FLOAT_PROP(CurvetoCubicSmoothRel, X, 2)
IMPL_FLOAT_PROP(CurvetoCubicSmoothRel, Y, 3)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegCurvetoQuadraticSmoothAbs
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegCurvetoQuadraticSmoothAbs
{
public:
  DOMSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = x;
    mArgs[1] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGCURVETOQUADRATICSMOOTHABS
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(CurvetoQuadraticSmoothAbs, PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS)

protected:
  float mArgs[2];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(CurvetoQuadraticSmoothAbs)

IMPL_FLOAT_PROP(CurvetoQuadraticSmoothAbs, X, 0)
IMPL_FLOAT_PROP(CurvetoQuadraticSmoothAbs, Y, 1)


////////////////////////////////////////////////////////////////////////

class DOMSVGPathSegCurvetoQuadraticSmoothRel
  : public DOMSVGPathSeg
  , public nsIDOMSVGPathSegCurvetoQuadraticSmoothRel
{
public:
  DOMSVGPathSegCurvetoQuadraticSmoothRel(float x, float y)
    : DOMSVGPathSeg()
  {
    mArgs[0] = x;
    mArgs[1] = y;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSVGPATHSEGCURVETOQUADRATICSMOOTHREL
  IMPL_SVGPATHSEG_SUBCLASS_COMMON(CurvetoQuadraticSmoothRel, PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL)

protected:
  float mArgs[2];
};

IMPL_NSISUPPORTS_SVGPATHSEG_SUBCLASS(CurvetoQuadraticSmoothRel)

IMPL_FLOAT_PROP(CurvetoQuadraticSmoothRel, X, 0)
IMPL_FLOAT_PROP(CurvetoQuadraticSmoothRel, Y, 1)



// This must come after DOMSVGPathSegClosePath et. al. have been declared.
/* static */ DOMSVGPathSeg*
DOMSVGPathSeg::CreateFor(DOMSVGPathSegList *aList,
                         PRUint32 aListIndex,
                         bool aIsAnimValItem)
{
  PRUint32 dataIndex = aList->mItems[aListIndex].mInternalDataIndex;
  float *data = &aList->InternalList().mData[dataIndex];
  PRUint32 type = SVGPathSegUtils::DecodeType(data[0]);

  switch (type)
  {
  case nsIDOMSVGPathSeg::PATHSEG_CLOSEPATH:
    return new DOMSVGPathSegClosePath(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_MOVETO_ABS:
    return new DOMSVGPathSegMovetoAbs(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_MOVETO_REL:
    return new DOMSVGPathSegMovetoRel(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_LINETO_ABS:
    return new DOMSVGPathSegLinetoAbs(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_LINETO_REL:
    return new DOMSVGPathSegLinetoRel(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_ABS:
    return new DOMSVGPathSegCurvetoCubicAbs(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_REL:
    return new DOMSVGPathSegCurvetoCubicRel(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_ABS:
    return new DOMSVGPathSegCurvetoQuadraticAbs(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_REL:
    return new DOMSVGPathSegCurvetoQuadraticRel(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_ARC_ABS:
    return new DOMSVGPathSegArcAbs(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_ARC_REL:
    return new DOMSVGPathSegArcRel(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_ABS:
    return new DOMSVGPathSegLinetoHorizontalAbs(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_LINETO_HORIZONTAL_REL:
    return new DOMSVGPathSegLinetoHorizontalRel(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_ABS:
    return new DOMSVGPathSegLinetoVerticalAbs(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_LINETO_VERTICAL_REL:
    return new DOMSVGPathSegLinetoVerticalRel(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
    return new DOMSVGPathSegCurvetoCubicSmoothAbs(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
    return new DOMSVGPathSegCurvetoCubicSmoothRel(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
    return new DOMSVGPathSegCurvetoQuadraticSmoothAbs(aList, aListIndex, aIsAnimValItem);
  case nsIDOMSVGPathSeg::PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
    return new DOMSVGPathSegCurvetoQuadraticSmoothRel(aList, aListIndex, aIsAnimValItem);
  default:
    NS_NOTREACHED("Invalid path segment type");
    return nullptr;
  }
}




nsIDOMSVGPathSeg*
NS_NewSVGPathSegClosePath()
{
  return new DOMSVGPathSegClosePath();
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegMovetoAbs(float x, float y)
{
  return new DOMSVGPathSegMovetoAbs(x, y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegMovetoRel(float x, float y)
{
  return new DOMSVGPathSegMovetoRel(x, y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoAbs(float x, float y)
{
  return new DOMSVGPathSegLinetoAbs(x, y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoRel(float x, float y)
{
  return new DOMSVGPathSegLinetoRel(x, y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoCubicAbs(float x, float y,
                                float x1, float y1,
                                float x2, float y2)
{
  // Note that we swap from DOM API argument order to the argument order used
  // in the <path> element's 'd' attribute (i.e. we put the arguments for the
  // end point of the segment last instead of first).

  return new DOMSVGPathSegCurvetoCubicAbs(x1, y1, x2, y2, x, y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoCubicRel(float x, float y,
                                float x1, float y1,
                                float x2, float y2)
{
  // See comment in NS_NewSVGPathSegCurvetoCubicAbs!

  return new DOMSVGPathSegCurvetoCubicRel(x1, y1, x2, y2, x, y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoQuadraticAbs(float x, float y,
                                    float x1, float y1)
{
  // See comment in NS_NewSVGPathSegCurvetoCubicAbs!

  return new DOMSVGPathSegCurvetoQuadraticAbs(x1, y1, x, y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoQuadraticRel(float x, float y,
                                    float x1, float y1)
{
  // See comment in NS_NewSVGPathSegCurvetoCubicAbs!

  return new DOMSVGPathSegCurvetoQuadraticRel(x1, y1, x, y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegArcAbs(float x, float y,
                       float r1, float r2, float angle,
                       bool largeArcFlag, bool sweepFlag)
{
  // See comment in NS_NewSVGPathSegCurvetoCubicAbs!

  return new DOMSVGPathSegArcAbs(r1, r2, angle, largeArcFlag, sweepFlag, x, y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegArcRel(float x, float y,
                       float r1, float r2, float angle,
                       bool largeArcFlag, bool sweepFlag)
{
  // See comment in NS_NewSVGPathSegCurvetoCubicAbs!

  return new DOMSVGPathSegArcRel(r1, r2, angle, largeArcFlag, sweepFlag, x, y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoHorizontalAbs(float x)
{
  return new DOMSVGPathSegLinetoHorizontalAbs(x);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoHorizontalRel(float x)
{
  return new DOMSVGPathSegLinetoHorizontalRel(x);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoVerticalAbs(float y)
{
  return new DOMSVGPathSegLinetoVerticalAbs(y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegLinetoVerticalRel(float y)
{
  return new DOMSVGPathSegLinetoVerticalRel(y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoCubicSmoothAbs(float x, float y,
                                      float x2, float y2)
{
  // See comment in NS_NewSVGPathSegCurvetoCubicAbs!

  return new DOMSVGPathSegCurvetoCubicSmoothAbs(x2, y2, x, y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoCubicSmoothRel(float x, float y,
                                      float x2, float y2)
{
  // See comment in NS_NewSVGPathSegCurvetoCubicAbs!

  return new DOMSVGPathSegCurvetoCubicSmoothRel(x2, y2, x, y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoQuadraticSmoothAbs(float x, float y)
{
  return new DOMSVGPathSegCurvetoQuadraticSmoothAbs(x, y);
}

nsIDOMSVGPathSeg*
NS_NewSVGPathSegCurvetoQuadraticSmoothRel(float x, float y)
{
  return new DOMSVGPathSegCurvetoQuadraticSmoothRel(x, y);
}

