/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
 
#ifndef __nsDialogParamBlock_h
#define __nsDialogParamBlock_h

#include "nsIDialogParamBlock.h"

// {4E4AAE11-8901-46cc-8217-DAD7C5415873}
#define NS_DIALOGPARAMBLOCK_CID \
 {0x4e4aae11, 0x8901, 0x46cc, {0x82, 0x17, 0xda, 0xd7, 0xc5, 0x41, 0x58, 0x73}}
#define NS_DIALOGPARAMBLOCK_CONTRACTID \
 "@mozilla.org/embedcomp/dialogparam;1"

class nsString;

class nsDialogParamBlock: public nsIDialogParamBlock
{
public: 	
  nsDialogParamBlock();
  virtual ~nsDialogParamBlock();
   
  NS_DECL_NSIDIALOGPARAMBLOCK
  NS_DECL_ISUPPORTS	

private:

  enum {kNumInts = 8, kNumStrings = 16};

  nsresult InBounds(PRInt32 inIndex, PRInt32 inMax) {
    return inIndex >= 0 && inIndex < inMax ? NS_OK : NS_ERROR_ILLEGAL_VALUE;
  }
  
  PRInt32 mInt[kNumInts];
  PRInt32 mNumStrings;
  nsString* mString;  	
};

#endif

