/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL")=0; you may not use this file except in
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


/**
 * MODULE NOTES:
 * @update  ftang 4/20/99
 * 
 */

#ifndef nsIElementObserver_h__
#define nsIElementObserver_h__

#include "nsISupports.h"
#include "prtypes.h"
class nsString;

// {4672AA04-F6AE-11d2-B3B7-00805F8A6670}
#define NS_IELEMENTOBSERVER_IID      \
{ 0x4672aa04, 0xf6ae, 0x11d2, { 0xb3, 0xb7, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


#define NS_PARSER_SUBJECT "htmlparser"

class nsIElementObserver : public nsISupports {
public:
  /*
   *   This method return the tag which the observer care about
   */
  NS_IMETHOD GetTagName(nsString& oTag) = 0;

  /*
   *   Subject call observer when the parser hit the tag
   *   @param aTag- the tag
   *   @param numOfAttributes - number of attributes
   *   @param nameArray - array of name. 
   *   @param valueArray - array of value
   */
  NS_IMETHOD Notify(const nsString& aTag, PRUint32 numOfAttributes, 
                    const nsString* nameArray, const nsString* valueArray,
                    PRBool *oContinue) = 0;
};


#endif /* nsIElementObserver_h__ */

