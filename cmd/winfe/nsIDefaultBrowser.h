/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
   Version 1.0 (the "License"); you may not use this file except in
   compliance with the License. You may obtain a copy of the License at
   http://www.mozilla.org/NPL/ 

   Software distributed under the License is distributed on an "AS IS" basis,
   WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
   for the specific language governing rights and limitations under the License. 

   The Original Code is Mozilla Communicator client code, released March 31, 1998. 

   The Initial Developer of the Original Code is Netscape Communications Corporation.
   Portions created by Netscape are Copyright (C) 1998 Netscape Communications Corporation.
   All Rights Reserved.
 */
#ifndef _NSIDEFAULTBROWSER_H_
#define _NSIDEFAULTBROWSER_H_

#include "nsISupports.h"

// {C2DACA60-3162-11d2-8049-00600811A9C3}
#define NS_IDEFAULTBROWSER_IID      \
    { 0xc2daca60, 0x3162, 0x11d2, { 0x80, 0x49, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3 } };

/* nsIDefaultBrowser:   
 *
 *
 *
 *
 */
class nsIDefaultBrowser : public nsISupports {
  public:
    // GetInterface - static function to be used to get singleton object of this class.
    static nsIDefaultBrowser *GetInterface();

    // IsDefaultBrowser - return TRUE iff this application is registered as the "default
    //                    browser"
    NS_IMETHOD_(PRBool) IsDefaultBrowser() = 0;

    // DisplayDialog - displays the "default browser" dialog (intended for browser startup).
    NS_IMETHOD_(int)    DisplayDialog() = 0;
};

#endif // _NSIDEFAULTBROWSER_H_
