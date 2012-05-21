/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef __NS_ISVGGLYPHFRAGMENTNODE_H__
#define __NS_ISVGGLYPHFRAGMENTNODE_H__

#include "nsQueryFrame.h"

class nsIDOMSVGPoint;
class nsSVGGlyphFrame;

class nsISVGGlyphFragmentNode : public nsQueryFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsISVGGlyphFragmentNode)

  virtual PRUint32 GetNumberOfChars()=0;
  virtual float GetComputedTextLength()=0;
  virtual float GetSubStringLength(PRUint32 charnum, PRUint32 fragmentChars)=0;
  virtual PRInt32 GetCharNumAtPosition(nsIDOMSVGPoint *point)=0;
  NS_IMETHOD_(nsSVGGlyphFrame *) GetFirstGlyphFrame()=0;
  NS_IMETHOD_(nsSVGGlyphFrame *) GetNextGlyphFrame()=0;
  NS_IMETHOD_(void) SetWhitespaceCompression(bool aCompressWhitespace)=0;
};

#endif // __NS_ISVGGLYPHFRAGMENTNODE_H__
