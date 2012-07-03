/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "SVGTransformListParser.h"
#include "SVGTransform.h"
#include "prdtoa.h"
#include "nsDOMError.h"
#include "nsGkAtoms.h"
#include "nsCRT.h"
#include "nsDOMClassInfoID.h"
#include "nsIAtom.h"

using namespace mozilla;

//----------------------------------------------------------------------
// private methods

nsresult
SVGTransformListParser::Match()
{
  mTransforms.Clear();
  return MatchTransformList();
}


nsresult
SVGTransformListParser::MatchTransformList()
{
  MatchWsp();

  if (IsTokenTransformStarter()) {
    ENSURE_MATCHED(MatchTransforms());
  }

  MatchWsp();

  return NS_OK;
}


nsresult
SVGTransformListParser::MatchTransforms()
{
  ENSURE_MATCHED(MatchTransform());

  while (mTokenType != END) {
    const char* pos = mTokenPos;

    /* Curiously the SVG BNF allows multiple comma-wsp between transforms */
    while (IsTokenCommaWspStarter()) {
      ENSURE_MATCHED(MatchCommaWsp());
    }

    if (IsTokenTransformStarter()) {
      ENSURE_MATCHED(MatchTransform());
    } else {
      if (pos != mTokenPos) RewindTo(pos);
      break;
    }
  }

  return NS_OK;
}


nsresult
SVGTransformListParser::GetTransformToken(nsIAtom** aKeyAtom,
                                          bool aAdvancePos)
{
  if (mTokenType != OTHER || *mTokenPos == '\0') {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;

  const char* delimiters = "\x20\x9\xD\xA,(";
  char* delimiterStart = PL_strnpbrk(mTokenPos, delimiters, 11);
  if (delimiterStart != 0) {
    /* save this character and null it out */
    char holdingChar = *delimiterStart;
    *delimiterStart = '\0';

    PRUint32 len;
    if ((len = strlen(mTokenPos)) > 0) {
      *aKeyAtom = NS_NewAtom(Substring(mTokenPos, mTokenPos + len));

      if (aAdvancePos) {
         mInputPos = mTokenPos + len;
         mTokenPos = mInputPos;
      }
    } else {
      rv = NS_ERROR_FAILURE;
    }
    /* reset character back to original */
    *delimiterStart = holdingChar;
  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}


nsresult
SVGTransformListParser::MatchTransform()
{
  nsCOMPtr<nsIAtom> keyatom;

  nsresult rv = GetTransformToken(getter_AddRefs(keyatom), true);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (keyatom == nsGkAtoms::translate) {
    ENSURE_MATCHED(MatchTranslate());
  } else if (keyatom == nsGkAtoms::scale) {
    ENSURE_MATCHED(MatchScale());
  } else if (keyatom == nsGkAtoms::rotate) {
    ENSURE_MATCHED(MatchRotate());
  } else if (keyatom == nsGkAtoms::skewX) {
    ENSURE_MATCHED(MatchSkewX());
  } else if (keyatom == nsGkAtoms::skewY) {
    ENSURE_MATCHED(MatchSkewY());
  } else if (keyatom == nsGkAtoms::matrix) {
    ENSURE_MATCHED(MatchMatrix());
  } else {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


bool
SVGTransformListParser::IsTokenTransformStarter()
{
  nsCOMPtr<nsIAtom> keyatom;

  nsresult rv = GetTransformToken(getter_AddRefs(keyatom), false);
  if (NS_FAILED(rv)) {
    return false;
  }

  if (keyatom == nsGkAtoms::translate ||
      keyatom == nsGkAtoms::scale     ||
      keyatom == nsGkAtoms::rotate    ||
      keyatom == nsGkAtoms::skewX     ||
      keyatom == nsGkAtoms::skewY     ||
      keyatom == nsGkAtoms::matrix) {
    return true;
  }

  return false;
}

nsresult
SVGTransformListParser::MatchNumberArguments(float *aResult,
                                             PRUint32 aMaxNum,
                                             PRUint32 *aParsedNum)
{
  *aParsedNum = 0;

  MatchWsp();

  ENSURE_MATCHED(MatchLeftParen());

  MatchWsp();

  ENSURE_MATCHED(MatchNumber(&aResult[0]));
  *aParsedNum = 1;

  while (IsTokenCommaWspStarter()) {
    MatchWsp();
    if (mTokenType == RIGHT_PAREN) {
      break;
    }
    if (*aParsedNum == aMaxNum) {
      return NS_ERROR_FAILURE;
    }
    if (IsTokenCommaWspStarter()) {
      MatchCommaWsp();
    }
    ENSURE_MATCHED(MatchNumber(&aResult[(*aParsedNum)++]));
  }

  MatchWsp();

  ENSURE_MATCHED(MatchRightParen());

  return NS_OK;
}

nsresult
SVGTransformListParser::MatchTranslate()
{
  GetNextToken();

  float t[2];
  PRUint32 count;

  ENSURE_MATCHED(MatchNumberArguments(t, ArrayLength(t), &count));

  switch (count) {
    case 1:
      t[1] = 0.f;
      // fall-through
    case 2:
    {
      SVGTransform* transform = mTransforms.AppendElement();
      NS_ENSURE_TRUE(transform, NS_ERROR_OUT_OF_MEMORY);
      transform->SetTranslate(t[0], t[1]);
      break;
    }
    default:
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


nsresult
SVGTransformListParser::MatchScale()
{
  GetNextToken();

  float s[2];
  PRUint32 count;

  ENSURE_MATCHED(MatchNumberArguments(s, ArrayLength(s), &count));

  switch (count) {
    case 1:
      s[1] = s[0];
      // fall-through
    case 2:
    {
      SVGTransform* transform = mTransforms.AppendElement();
      NS_ENSURE_TRUE(transform, NS_ERROR_OUT_OF_MEMORY);
      transform->SetScale(s[0], s[1]);
      break;
    }
    default:
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


nsresult
SVGTransformListParser::MatchRotate()
{
  GetNextToken();

  float r[3];
  PRUint32 count;

  ENSURE_MATCHED(MatchNumberArguments(r, ArrayLength(r), &count));

  switch (count) {
    case 1:
      r[1] = r[2] = 0.f;
      // fall-through
    case 3:
    {
      SVGTransform* transform = mTransforms.AppendElement();
      NS_ENSURE_TRUE(transform, NS_ERROR_OUT_OF_MEMORY);
      transform->SetRotate(r[0], r[1], r[2]);
      break;
    }
    default:
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


nsresult
SVGTransformListParser::MatchSkewX()
{
  GetNextToken();

  float skew;
  PRUint32 count;

  ENSURE_MATCHED(MatchNumberArguments(&skew, 1, &count));

  if (count != 1) {
    return NS_ERROR_FAILURE;
  }

  SVGTransform* transform = mTransforms.AppendElement();
  NS_ENSURE_TRUE(transform, NS_ERROR_OUT_OF_MEMORY);
  transform->SetSkewX(skew);

  return NS_OK;
}


nsresult
SVGTransformListParser::MatchSkewY()
{
  GetNextToken();

  float skew;
  PRUint32 count;

  ENSURE_MATCHED(MatchNumberArguments(&skew, 1, &count));

  if (count != 1) {
    return NS_ERROR_FAILURE;
  }

  SVGTransform* transform = mTransforms.AppendElement();
  NS_ENSURE_TRUE(transform, NS_ERROR_OUT_OF_MEMORY);
  transform->SetSkewY(skew);

  return NS_OK;
}


nsresult
SVGTransformListParser::MatchMatrix()
{
  GetNextToken();

  float m[6];
  PRUint32 count;

  ENSURE_MATCHED(MatchNumberArguments(m, ArrayLength(m), &count));

  if (count != 6) {
    return NS_ERROR_FAILURE;
  }

  SVGTransform* transform = mTransforms.AppendElement();
  NS_ENSURE_TRUE(transform, NS_ERROR_OUT_OF_MEMORY);
  transform->SetMatrix(gfxMatrix(m[0], m[1], m[2], m[3], m[4], m[5]));

  return NS_OK;
}
