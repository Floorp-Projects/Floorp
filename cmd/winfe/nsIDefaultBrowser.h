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

    // Prefs - nested structure defining default-browser related prefs.
    struct Prefs {
        PRBool bHandleFiles;
        PRBool bHandleShortcuts;
        PRBool bIntegrateWithActiveDesktop;
        // Internet shortcut protocols.
        struct {
            PRBool bHandleHTTP;
            PRBool bHandleHTTPS;
            PRBool bHandleFTP;
        };
        // File types.
        struct {
            PRBool bHandleHTML;
            PRBool bHandleJPEG;
            PRBool bHandleGIF;
            PRBool bHandleJS;
            PRBool bHandleXBM;
            PRBool bHandleTXT;
        };
        // "Active desktop" stuff.
        struct {
            PRBool bUseInternetKeywords;
            PRBool bUseNetcenterSearch;
            PRBool bDisableActiveDesktop;
        };
    };

    // GetPreferences - Returns Prefs structure filled in with user's preferences.
    NS_IMETHOD GetPreferences( Prefs &prefs ) = 0;

    // SetPreferences - Sets user preferences from Prefs structure contents.
    NS_IMETHOD SetPreferences( const Prefs &prefs ) = 0;

    // Things - enumeration of protocols/files that Mozilla can handle.
    enum Thing {
        kHTTP, kHTTPS, kFTP, kMaxProtocol = kFTP,
        kHTML, kJPEG, kGIF, kJS, kXBM, kTXT, kMaxFileType = kTXT,
        kInternetKeywords, kNetcenterSearch, kActiveDesktop
    };

    // IsHandling - returns whether Mozilla is handling a given thing.
    NS_IMETHOD_(PRBool) IsHandling( Thing thing ) = 0;

    // StartHandling - does necessary stuff to have Mozilla take over handling of a
    //                 given thing.
    NS_IMETHOD StartHandling( Thing extOrProtocol ) = 0;

    // StopHandling - vice versa
    NS_IMETHOD StopHandling( Thing extOrProtocol ) = 0;

    // HandlePerPreferences - Apply preferences to registry.
    NS_IMETHOD HandlePerPreferences() = 0;
};

#endif // _NSIDEFAULTBROWSER_H_
