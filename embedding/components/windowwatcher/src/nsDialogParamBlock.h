/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#ifndef __nsDialogParamBlock_h
#define __nsDialogParamBlock_h

#include "nsIDialogParamBlock.h"
#include "nsIMutableArray.h"
#include "nsCOMPtr.h"

// {4E4AAE11-8901-46cc-8217-DAD7C5415873}
#define NS_DIALOGPARAMBLOCK_CID \
 {0x4e4aae11, 0x8901, 0x46cc, {0x82, 0x17, 0xda, 0xd7, 0xc5, 0x41, 0x58, 0x73}}

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
  nsCOMPtr<nsIMutableArray> mObjects;	
};

#endif

