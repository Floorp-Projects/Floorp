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

#ifndef nsInputCheckbox_h___
#define nsInputCheckbox_h___

#include "nsInput.h"
class nsIAtom;
class nsString;

class nsInputCheckbox : public nsInput {
public:
  typedef nsInput super;
  nsInputCheckbox (nsIAtom* aTag, nsIFormManager* aManager);

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

protected:
  virtual ~nsInputCheckbox();

  virtual void GetType(nsString& aResult) const;

   PRBool mChecked;               // initial checked flag value
};

#endif
