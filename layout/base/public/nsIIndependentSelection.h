/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

/*
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * NOTE!!  This is not a general class, but specific to layout and frames.
 * Consumers looking for the general selection interface should look at
 * nsISelection.
 */


#ifndef nsIIndependentSelection_h___
#define nsIIndependentSelection_h___

#include "nsISupports.h"




#define NS_IINDEPENDENTSELECTION_IID      \
/* {3B7ABF61-33DB-4a47-8FFA-FFD6EA47CB1A} */ \
{ 0x3b7abf61, 0x33db, 0x4a47, \
  { 0x8f, 0xfa, 0xff, 0xd6, 0xea, 0x47, 0xcb, 0x1a } }



/*This interface is used to allow nsDOMSelection become more of an independent entity from nsSelection. */
class nsIIndependentSelection: public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IINDEPENDENTSELECTION_IID; return iid; }

  /** SetPresShell
   *  this method sets the internal pres shell to aPresShell
   *  @param aPresShell weak reference should be kept to this.
   */
  NS_IMETHOD SetPresShell(nsIPresShell *aPresShell) =0;
};

#endif //nsIIndependentSelection_h___
