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
 * Contributor(s): 
 */
#ifndef nsIMutableStyleContext_h___
#define nsIMutableStyleContext_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsStyleStruct.h"


#define NS_IMUTABLESTYLECONTEXT_IID   \
 { 0x53cbb100, 0x8340, 0x11d3, \
   {0xba, 0x05, 0x00, 0x10, 0x83, 0x02, 0x3c, 0x2b} }


class nsIMutableStyleContext : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IMUTABLESTYLECONTEXT_IID; return iid; }

  virtual nsIStyleContext*  GetParent(void) const = 0;

  // Fill a style struct with data
  NS_IMETHOD GetStyle(nsStyleStructID aSID, nsStyleStruct& aStruct) const = 0;
  NS_IMETHOD SetStyle(nsStyleStructID aSID, const nsStyleStruct& aStruct) = 0;

  //------------------------------------------------
  // TEMP methods these are here only to ease the transition
  // to the newer Get/SetStyle APIs above. Don't call these.
  // get a style data struct by ID
  virtual const nsStyleStruct* GetStyleData(nsStyleStructID aSID) = 0;
  virtual nsStyleStruct* GetMutableStyleData(nsStyleStructID aSID) = 0;
};

#endif /* nsIMutableStyleContext_h___ */
