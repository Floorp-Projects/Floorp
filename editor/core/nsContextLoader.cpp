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

#include "nsIContextLoader.h"


struct nsCtxTupple
{
  PRUint32 mI1;
  PRUint32 mI2;
  PRUint32 mResult;
};

//this here line don't compile:
//static nsCtxTupple *gCtxTable={{},{},{}};

class nsEditorDefaultLoader  : public nsIContextLoader{
public:

  virtual nsresult Lookup(PRUint32 aIndex1, PRUint32 aIndex2, PRUint32 **aResult)= 0;

private:
  nsCtxTupple *mCtxArray;
};




nsresult NS_MakeEditorLoader(nsIContextLoader **aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

