/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsIStyleContext_h___
#define nsIStyleContext_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsMargin.h"
#include "nsFont.h"
class nsIFrame;
class nsIPresContext;
class nsIContent;
class nsISupportsArray;

// SID e31e1bc0-ca9b-11d1-8031-006008159b5a
#define NS_STYLEFONT_SID   \
{0xe31e1bc0, 0xca9b, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

// SID 05953860-ca9c-11d1-8031-006008159b5a
#define NS_STYLECOLOR_SID   \
{0x05953860, 0xca9c, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

// SID 4fb83b60-cf27-11d1-8031-006008159b5a
#define NS_STYLELIST_SID   \
{0x4fb83b60, 0xcf27, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

// Indicies into border/padding/margin arrays
#define NS_SIDE_TOP     0
#define NS_SIDE_RIGHT   1
#define NS_SIDE_BOTTOM  2
#define NS_SIDE_LEFT    3

// The lifetime of these objects is managed by the nsIStyleContext.

struct nsStyleStruct {
  virtual const nsID& GetID(void) = 0;
};

struct nsStyleFont : public nsStyleStruct {
  nsFont  mFont;
  PRUint8 mThreeD;  // XXX fold this into nsFont

protected:
  nsStyleFont(const nsFont& aFont);
  ~nsStyleFont(void);
};

struct nsStyleColor : public nsStyleStruct {
  nscolor mColor;
 
  PRUint8 mBackgroundAttachment;        // See nsStyleConsts.h
  PRUint8 mBackgroundFlags;             // See nsStyleConsts.h
  PRUint8 mBackgroundRepeat;            // See nsStyleConsts.h

  nscolor mBackgroundColor;
  nscoord mBackgroundXPosition;
  nscoord mBackgroundYPosition;
  nsString mBackgroundImage;            // absolute url string
};

struct nsStyleList : public nsStyleStruct {
  PRUint8   mListStyleType;             // See nsStyleConsts.h
  nsString  mListStyleImage;            // absolute url string
  PRUint8   mListStylePosition;
};

//----------------------------------------------------------------------
// XXX begin temporary doomed code

// This data structure is doomed: it's a temporary hack for catch-all
// style that hasn't yet been factored properly into nsStyleX
// struct's.

// SID 05fb99f0-ca9c-11d1-8031-006008159b5a
#define NS_STYLEMOLECULE_SID    \
{0x05fb99f0, 0xca9c, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

struct nsStyleMolecule : public nsStyleStruct {
  PRUint8 borderSizeFlags[4];
  nsMargin border;
  PRUint8 borderStyle[4];
  nscolor borderColor[4];
  PRUint8 clear;
  PRUint8 clipFlags;
  PRUint8 cursor;
  PRUint8 display;
  PRUint8 direction;
  PRUint8 floats;
  nsMargin margin;
  nsMargin padding;
  nsMargin borderPadding;
  PRUint32 textDecoration;
  PRUint8 textAlign;
  PRInt32 whiteSpace;
  PRUint8 verticalAlign;
  float verticalAlignPct;
  PRInt32 fixedWidth;
  PRInt32 proportionalWidth;
  PRInt32 fixedHeight;
  PRInt32 proportionalHeight;
  PRUint8 positionFlags;
  nscoord top, left;

protected:
  // The constructor is protected because you may not make these
  // automatic variables.
  nsStyleMolecule();
  ~nsStyleMolecule();

private:
  // These are not allowed
  nsStyleMolecule(const nsStyleMolecule& other);
  nsStyleMolecule& operator=(const nsStyleMolecule& other);
};

// XXX end temporary doomed code
//----------------------------------------------------------------------

#define NS_ISTYLECONTEXT_IID   \
 { 0x26a4d970, 0xa342, 0x11d1, \
   {0x89, 0x74, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

class nsIStyleContext : public nsISupports {
public:
  virtual PRBool    Equals(const nsIStyleContext* aOther) const = 0;
  virtual PRUint32  HashValue(void) const = 0;

  virtual nsIStyleContext*  GetParent(void) const = 0;
  virtual nsISupportsArray* GetStyleRules(void) const = 0;

  // get a style data struct by ID, may return null 
  virtual nsStyleStruct* GetData(const nsIID& aSID) = 0;
};

// this is private to nsStyleSet, don't call it
extern NS_LAYOUT nsresult
  NS_NewStyleContext(nsIStyleContext** aInstancePtrResult,
                     nsISupportsArray* aRules,
                     nsIPresContext* aPresContext,
                     nsIContent* aContent,
                     nsIFrame* aParentFrame);

#endif /* nsIStyleContext_h___ */
