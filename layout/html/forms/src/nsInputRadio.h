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

#ifndef nsInputRadio_h___
#define nsInputRadio_h___

// this class defintion will be moved into nsInputRadio.cpp

#include "nsInput.h"
#include "nsVoidArray.h"
class nsIAtom;
class nsString;

class nsInputRadio : public nsInput 
{
public:
  typedef nsInput nsInputRadioSuper;
  nsInputRadio (nsIAtom* aTag, nsIFormManager* aManager);

  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aResult) const;

  static const nsString* kTYPE;

  virtual PRBool GetChecked(PRBool aGetInitialValue) const;
  virtual void SetChecked(PRBool aValue, PRBool aSetInitialValue);

  virtual PRInt32 GetMaxNumValues() { return 1; }
  
  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  virtual void MapAttributesInto(nsIStyleContext* aContext, 
                                 nsIPresContext* aPresContext);
  virtual void Reset();

protected:
  virtual ~nsInputRadio();

  virtual void GetType(nsString& aResult) const;

  PRBool mInitialChecked;
  PRBool mForcedChecked;   

  friend class nsInputRadioFrame;
};

class nsInputRadioGroup
{
public:
  nsInputRadioGroup(nsString& aName);
  virtual ~nsInputRadioGroup();

  PRBool          AddRadio(nsIFormControl* aRadio);
  PRInt32         GetRadioCount() const;
  nsIFormControl* GetRadioAt(PRInt32 aIndex) const;
  PRBool          RemoveRadio(nsIFormControl* aRadio);

  nsIFormControl* GetCheckedRadio();
  void            SetCheckedRadio(nsIFormControl* aRadio);
  void            GetName(nsString& aNameResult) const;

protected:
  nsString        mName;
  nsVoidArray     mRadios;
  nsIFormControl* mCheckedRadio;
};

#endif


