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
#ifndef nsHTMLForms_h___
#define nsHTMLForms_h___

#include "nsIHTMLContent.h"
class nsIAtom;
class nsIFormManager;

// Form and Form Controls

/** 
  * Construct an object implementing nsIFormManager 
  * @param aInstancePtrResult the address at which to place the
  * address of the new instance.
  * @param aTag the html tag which corresponds to the nsIFormManager
  * @return NS_OK if the object was successfully constructed. 
  */
extern nsresult
NS_NewHTMLForm(nsIFormManager** aInstancePtrResult,
               nsIAtom* aTag);

/** 
  * Construct an object with behavior of an html input button
  * @param aInstancePtrResult the address at which to place the
  * address of the new instance.
  * @param aTag the html tag which corresponds to the nsIFormManager
  * @param aManager the form manager which will manage the constructed object
  * @return NS_OK if the object was successfully constructed. 
  */
extern nsresult
NS_NewHTMLInputButton(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag, nsIFormManager* aManager);

/** 
  * Construct an nsIHTMLContent with behavior of an html reset button
  * @see NS_NewHTMLInputButton for parameter and return values
  */
extern nsresult
NS_NewHTMLInputReset(nsIHTMLContent** aInstancePtrResult,
                     nsIAtom* aTag, nsIFormManager* aManager);

/** 
  * Construct an nsIHTMLContent with behavior of an html submit button
  * @see NS_NewHTMLInputButton for parameter and return values
  */
extern nsresult
NS_NewHTMLInputSubmit(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag, nsIFormManager* aManager);

/** 
  * Construct an nsIHTMLContent with behavior of an html checkbox 
  * @see NS_NewHTMLInputButton for parameter and return values
  */
extern nsresult
NS_NewHTMLInputCheckbox(nsIHTMLContent** aInstancePtrResult,
                        nsIAtom* aTag, nsIFormManager* aManager);

/** 
  * Construct an nsIHTMLContent with behavior of an html input file
  * @see NS_NewHTMLInputButton for parameter and return values
  */
extern nsresult
NS_NewHTMLInputFile(nsIHTMLContent** aInstancePtrResult,
                    nsIAtom* aTag, nsIFormManager* aManager);

/** 
  * Construct an nsIHTMLContent with behavior of an html input hidden
  * @see NS_NewHTMLInputButton for parameter and return values
  */
extern nsresult
NS_NewHTMLInputHidden(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag, nsIFormManager* aManager);

/** 
  * Construct an nsIHTMLContent with behavior of an html input image
  * @see NS_NewHTMLInputButton for parameter and return values
  */
extern nsresult
NS_NewHTMLInputImage(nsIHTMLContent** aInstancePtrResult,
                     nsIAtom* aTag, nsIFormManager* aManager);

/** 
  * Construct an nsIHTMLContent with behavior of an html input password
  * @see NS_NewHTMLInputButton for parameter and return values
  */
extern nsresult
NS_NewHTMLInputPassword(nsIHTMLContent** aInstancePtrResult,
                        nsIAtom* aTag, nsIFormManager* aManager);

/** 
  * Construct an nsIHTMLContent with behavior of an html input radio
  * @see NS_NewHTMLInputButton for parameter and return values
  */
extern nsresult
NS_NewHTMLInputRadio(nsIHTMLContent** aInstancePtrResult,
                     nsIAtom* aTag, nsIFormManager* aManager);

/** 
  * Construct an nsIHTMLContent with behavior of an html input text
  * @see NS_NewHTMLInputButton for parameter and return values
  */
extern nsresult
NS_NewHTMLInputText(nsIHTMLContent** aInstancePtrResult,
                    nsIAtom* aTag, nsIFormManager* aManager);

#endif /* nsHTMLForms_h___ */
