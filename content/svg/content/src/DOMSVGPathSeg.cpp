/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "DOMSVGPathSeg.h"
#include "DOMSVGPathSegList.h"
#include "SVGAnimatedPathSegList.h"
#include "nsSVGElement.h"
#include "nsError.h"
#include "nsContentUtils.h"

// See the architecture comment in DOMSVGPathSegList.h.

using namespace mozilla;

// We could use NS_IMPL_CYCLE_COLLECTION_1, except that in Unlink() we need to
// clear our list's weak ref to us to be safe. (The other option would be to
// not unlink and rely on the breaking of the other edges in the cycle, as
// NS_SVG_VAL_IMPL_CYCLE_COLLECTION does.)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMSVGPathSeg)
  // We may not belong to a list, so we must null check tmp->mList.
  if (tmp->mList) {
    tmp->mList->ItemAt(tmp->mListIndex) = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK(mList)
NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMSVGPathSeg)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DOMSVGPathSeg)
NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGPathSeg)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGPathSeg)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGPathSeg)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


DOMSVGPathSeg::DOMSVGPathSeg(DOMSVGPathSegList *aList,
                             uint32_t aListIndex,
                             bool aIsAnimValItem)
  : mList(aList)
  , mListIndex(aListIndex)
  , mIsAnimValItem(aIsAnimValItem)
{
  SetIsDOMBinding();
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
  SetIsDOMBinding();
}

void
DOMSVGPathSeg::InsertingIntoList(DOMSVGPathSegList *aList,
                                 uint32_t aListIndex,
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
  uint32_t argCount = SVGPathSegUtils::ArgCountForType(Type());
  // InternalItem() + 1, because the args come after the encoded seg type
  memcpy(PtrToMemberArgs(), InternalItem() + 1, argCount * sizeof(float));
  mList = nullptr;
  mIsAnimValItem = false;
}

void
DOMSVGPathSeg::ToSVGPathSegEncodedData(float* aRaw)
{
  NS_ABORT_IF_FALSE(aRaw, "null pointer");
  uint32_t argCount = SVGPathSegUtils::ArgCountForType(Type());
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
  uint32_t dataIndex = mList->mItems[mListIndex].mInternalDataIndex;
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

#define IMPL_PROP_WITH_TYPE(segName, propName, index, type)                   \
  /* attribute type propName; */                                              \
  type                                                                        \
  DOMSVGPathSeg##segName::propName()                                          \
  {                                                                           \
    if (mIsAnimValItem && HasOwner()) {                                       \
      Element()->FlushAnimations(); /* May make HasOwner() == false */        \
    }                                                                         \
    return type(HasOwner() ? InternalItem()[1+index] : mArgs[index]);         \
  }                                                                           \
  void                                                                        \
  DOMSVGPathSeg##segName::Set##propName(type a##propName, ErrorResult& rv)    \
  {                                                                           \
    if (mIsAnimValItem) {                                                     \
      rv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);                     \
      return;                                                                 \
    }                                                                         \
    if (HasOwner()) {                                                         \
      if (InternalItem()[1+index] == float(a##propName)) {                    \
        return;                                                               \
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
  }

// For float, the normal type of arguments
#define IMPL_FLOAT_PROP(segName, propName, index) \
  IMPL_PROP_WITH_TYPE(segName, propName, index, float)

// For the boolean flags in arc commands
#define IMPL_BOOL_PROP(segName, propName, index) \
  IMPL_PROP_WITH_TYPE(segName, propName, index, bool)


///////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(MovetoAbs, X, 0)
IMPL_FLOAT_PROP(MovetoAbs, Y, 1)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(MovetoRel, X, 0)
IMPL_FLOAT_PROP(MovetoRel, Y, 1)



////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(LinetoAbs, X, 0)
IMPL_FLOAT_PROP(LinetoAbs, Y, 1)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(LinetoRel, X, 0)
IMPL_FLOAT_PROP(LinetoRel, Y, 1)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(CurvetoCubicAbs, X1, 0)
IMPL_FLOAT_PROP(CurvetoCubicAbs, Y1, 1)
IMPL_FLOAT_PROP(CurvetoCubicAbs, X2, 2)
IMPL_FLOAT_PROP(CurvetoCubicAbs, Y2, 3)
IMPL_FLOAT_PROP(CurvetoCubicAbs, X, 4)
IMPL_FLOAT_PROP(CurvetoCubicAbs, Y, 5)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(CurvetoCubicRel, X1, 0)
IMPL_FLOAT_PROP(CurvetoCubicRel, Y1, 1)
IMPL_FLOAT_PROP(CurvetoCubicRel, X2, 2)
IMPL_FLOAT_PROP(CurvetoCubicRel, Y2, 3)
IMPL_FLOAT_PROP(CurvetoCubicRel, X, 4)
IMPL_FLOAT_PROP(CurvetoCubicRel, Y, 5)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(CurvetoQuadraticAbs, X1, 0)
IMPL_FLOAT_PROP(CurvetoQuadraticAbs, Y1, 1)
IMPL_FLOAT_PROP(CurvetoQuadraticAbs, X, 2)
IMPL_FLOAT_PROP(CurvetoQuadraticAbs, Y, 3)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(CurvetoQuadraticRel, X1, 0)
IMPL_FLOAT_PROP(CurvetoQuadraticRel, Y1, 1)
IMPL_FLOAT_PROP(CurvetoQuadraticRel, X, 2)
IMPL_FLOAT_PROP(CurvetoQuadraticRel, Y, 3)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(ArcAbs, R1, 0)
IMPL_FLOAT_PROP(ArcAbs, R2, 1)
IMPL_FLOAT_PROP(ArcAbs, Angle, 2)
IMPL_BOOL_PROP(ArcAbs, LargeArcFlag, 3)
IMPL_BOOL_PROP(ArcAbs, SweepFlag, 4)
IMPL_FLOAT_PROP(ArcAbs, X, 5)
IMPL_FLOAT_PROP(ArcAbs, Y, 6)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(ArcRel, R1, 0)
IMPL_FLOAT_PROP(ArcRel, R2, 1)
IMPL_FLOAT_PROP(ArcRel, Angle, 2)
IMPL_BOOL_PROP(ArcRel, LargeArcFlag, 3)
IMPL_BOOL_PROP(ArcRel, SweepFlag, 4)
IMPL_FLOAT_PROP(ArcRel, X, 5)
IMPL_FLOAT_PROP(ArcRel, Y, 6)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(LinetoHorizontalAbs, X, 0)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(LinetoHorizontalRel, X, 0)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(LinetoVerticalAbs, Y, 0)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(LinetoVerticalRel, Y, 0)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(CurvetoCubicSmoothAbs, X2, 0)
IMPL_FLOAT_PROP(CurvetoCubicSmoothAbs, Y2, 1)
IMPL_FLOAT_PROP(CurvetoCubicSmoothAbs, X, 2)
IMPL_FLOAT_PROP(CurvetoCubicSmoothAbs, Y, 3)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(CurvetoCubicSmoothRel, X2, 0)
IMPL_FLOAT_PROP(CurvetoCubicSmoothRel, Y2, 1)
IMPL_FLOAT_PROP(CurvetoCubicSmoothRel, X, 2)
IMPL_FLOAT_PROP(CurvetoCubicSmoothRel, Y, 3)


////////////////////////////////////////////////////////////////////////

IMPL_FLOAT_PROP(CurvetoQuadraticSmoothAbs, X, 0)
IMPL_FLOAT_PROP(CurvetoQuadraticSmoothAbs, Y, 1)


////////////////////////////////////////////////////////////////////////


IMPL_FLOAT_PROP(CurvetoQuadraticSmoothRel, X, 0)
IMPL_FLOAT_PROP(CurvetoQuadraticSmoothRel, Y, 1)



// This must come after DOMSVGPathSegClosePath et. al. have been declared.
/* static */ DOMSVGPathSeg*
DOMSVGPathSeg::CreateFor(DOMSVGPathSegList *aList,
                         uint32_t aListIndex,
                         bool aIsAnimValItem)
{
  uint32_t dataIndex = aList->mItems[aListIndex].mInternalDataIndex;
  float *data = &aList->InternalList().mData[dataIndex];
  uint32_t type = SVGPathSegUtils::DecodeType(data[0]);

  switch (type)
  {
  case PATHSEG_CLOSEPATH:
    return new DOMSVGPathSegClosePath(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_MOVETO_ABS:
    return new DOMSVGPathSegMovetoAbs(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_MOVETO_REL:
    return new DOMSVGPathSegMovetoRel(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_LINETO_ABS:
    return new DOMSVGPathSegLinetoAbs(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_LINETO_REL:
    return new DOMSVGPathSegLinetoRel(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_CURVETO_CUBIC_ABS:
    return new DOMSVGPathSegCurvetoCubicAbs(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_CURVETO_CUBIC_REL:
    return new DOMSVGPathSegCurvetoCubicRel(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_CURVETO_QUADRATIC_ABS:
    return new DOMSVGPathSegCurvetoQuadraticAbs(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_CURVETO_QUADRATIC_REL:
    return new DOMSVGPathSegCurvetoQuadraticRel(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_ARC_ABS:
    return new DOMSVGPathSegArcAbs(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_ARC_REL:
    return new DOMSVGPathSegArcRel(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_LINETO_HORIZONTAL_ABS:
    return new DOMSVGPathSegLinetoHorizontalAbs(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_LINETO_HORIZONTAL_REL:
    return new DOMSVGPathSegLinetoHorizontalRel(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_LINETO_VERTICAL_ABS:
    return new DOMSVGPathSegLinetoVerticalAbs(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_LINETO_VERTICAL_REL:
    return new DOMSVGPathSegLinetoVerticalRel(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_CURVETO_CUBIC_SMOOTH_ABS:
    return new DOMSVGPathSegCurvetoCubicSmoothAbs(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_CURVETO_CUBIC_SMOOTH_REL:
    return new DOMSVGPathSegCurvetoCubicSmoothRel(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS:
    return new DOMSVGPathSegCurvetoQuadraticSmoothAbs(aList, aListIndex, aIsAnimValItem);
  case PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL:
    return new DOMSVGPathSegCurvetoQuadraticSmoothRel(aList, aListIndex, aIsAnimValItem);
  default:
    NS_NOTREACHED("Invalid path segment type");
    return nullptr;
  }
}

