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

#ifndef nsInputText_h___
#define nsInputText_h___

#include "nsInput.h"
#include "nsString.h"
class nsIAtom;
class nsString;
class nsView;

// this class definition will move to nsInputText.cpp

class nsInputText : public nsInput {
public:
  nsInputText (nsIAtom* aTag, nsIFormManager* aManager);

  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aResult) const;

  virtual PRInt32 GetMaxNumValues();
  
  virtual PRBool GetValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                           nsString* aValues);

  virtual void Reset();

  // Note: this has a copy of code from nsInputFile
  nsString* mValue;
  PRInt32 mSize;
  PRInt32 mMaxLength;

protected:
  virtual ~nsInputText();

  virtual void GetType(nsString& aResult) const;
};

#endif


