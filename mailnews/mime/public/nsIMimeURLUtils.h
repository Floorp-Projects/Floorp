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

/********************************************************************************************************
 
     Wrapper class for various parsing URL parsing routines...
 
*********************************************************************************************************/

#ifndef _nsIMimeURLUtils_h__
#define _nsIMimeURLUtils_h__

#include "nsISupports.h" 

// {9C4DA768-07EB-11d3-8EE5-00A024669799}
#define NS_IMIME_URLUTILS_IID \
    { 0x9c4da768, 0x7eb, 0x11d3,              \
    { 0x8e, 0xe5, 0x0, 0xa0, 0x24, 0x66, 0x97, 0x99 } };

// {9C4DA772-07EB-11d3-8EE5-00A024669799}
#define NS_IMIME_URLUTILS_CID \
    { 0x9c4da772, 0x7eb, 0x11d3, \
    { 0x8e, 0xe5, 0x0, 0xa0, 0x24, 0x66, 0x97, 0x99 } };


class nsIMimeURLUtils : public nsISupports {
public: 

  static const nsIID& IID(void) { static nsIID iid = NS_IMIME_URLUTILS_IID; return iid; }

  NS_IMETHOD    URLType(const char *URL, PRInt32  *retType) = 0;

  NS_IMETHOD    ReduceURL (char *url, char **retURL) = 0;

  NS_IMETHOD    ScanForURLs(const char *input, int32 input_size,
                        char *output, int output_size, PRBool urls_only) = 0;

  NS_IMETHOD    MakeAbsoluteURL(char * absolute_url, char * relative_url, char **retURL) = 0;
}; 

#endif /* _nsIMimeURLUtils_h__ */
