/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#ifndef __nsIColorPicker_h__
#define __nsIColorPicker_h__

#include "nsISupports.h"
#include "nsrootidl.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"

/* starting interface:    nsIColorPicker */

#define NS_ICOLORPICKER_IID_STR "ed133d04-1dd1-11b2-957f-a04e70608d6e"

#define NS_ICOLORPICKER_IID \
  {0xed133d04, 0x1dd1, 0x11b2, \
    { 0x95, 0x7f, 0xa0, 0x4e, 0x70, 0x60, 0x8d, 0x6e }}

class nsIColorPicker : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICOLORPICKER_IID)

  /* void Paint (in nsIPresContext aPresContext, in nsIRenderingContext aRenderingContext); */
  NS_IMETHOD  Paint(nsIPresContext * aPresContext, nsIRenderingContext * aRenderingContext) = 0;

  /* void GetColor (in PRInt32 aX, in PRInt32 aY, out string aColor); */
  NS_IMETHOD  GetColor(PRInt32 aX, PRInt32 aY, char **aColor) = 0;

  /* void GetSize (out PRInt32 aWidth, out PRInt32 aHeight); */
  NS_IMETHOD  GetSize(PRInt32 *aWidth, PRInt32 *aHeight) = 0;
};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSICOLORPICKER \
  NS_IMETHOD  Paint(nsIPresContext * aPresContext, nsIRenderingContext * aRenderingContext); \
  NS_IMETHOD  GetColor(PRInt32 aX, PRInt32 aY, char **aColor); \
  NS_IMETHOD  GetSize(PRInt32 *aWidth, PRInt32 *aHeight); 

/* Use this macro to declare functions that forward the behavior of this interface to another object. */
#define NS_FORWARD_NSICOLORPICKER(_to) \
  NS_IMETHOD  Paint(nsIPresContext * aPresContext, nsIRenderingContext * aRenderingContext) { return _to ##  Paint(aPresContext, aRenderingContext); } \
  NS_IMETHOD  GetColor(PRInt32 aX, PRInt32 aY, char **aColor) { return _to ##  GetColor(aX, aY, aColor); } \
  NS_IMETHOD  GetSize(PRInt32 *aWidth, PRInt32 *aHeight) { return _to ##  GetSize(aWidth, aHeight); } 


#endif /* __nsIColorPicker_h__ */
