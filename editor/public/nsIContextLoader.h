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


#ifndef nsIContextLoader_h__
#define nsIContextLoader_h__

#include "nsISupports.h"

#define NS_ICONTEXTLOADER_IID \
{/* {2EFB86B0-8495-11d2-8F35-006008310194}*/ \
0x2efb86b0, 0x8495, 0x11d2, \
{ 0x8f, 0x35, 0x0, 0x60, 0x8, 0x31, 0x1, 0x94 } }



/**
 * nsIContextLoader will maintain a given array of context information
 * This context info will be indexed with 2 unsigned 32 bit integers
 * :(uint32, uint32) and will return another unsigned integer as the result.
 * <P>
 * Possible uses:  keycode lookup table for combination keys like <CNTRL><RETURN> 
 * will return something that will map to an enum that is important to someone
 *
 * actual implementation of this is opaque to the user of a nsIContextLoader
 * 
 * <P><P>unfinished interface so far
 */
class nsIContextLoader  : public nsISupports{
 public:

  static const nsIID& IID() { static nsIID iid = NS_ICONTEXTLOADER_IID; return iid; }

  /**
   * Lookup(PRUint32 aIndex1, PRUint32 aIndex2, PRUint32 **aResult)
   * @param aIndex1 first index to lookup the result
   * @param aIndex2 second index to lookup the result
   * @param aResult place to put the result of the lookup
   */
  virtual nsresult Lookup(PRUint32 aIndex1, PRUint32 aIndex2, PRUint32 **aResult)= 0;

};

#endif //nsIContextLoader