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
#include "nsIDefaultBrowser.h"

#include "stdafx.h"
#include "dialog.h"

class nsWinDefaultBrowser;

static nsWinDefaultBrowser *pSingleton = NULL;

// Implementation of the abstract (XP) "default browser" interface for Windows.
class nsWinDefaultBrowser : public nsIDefaultBrowser {
    NS_DECL_ISUPPORTS

  public:
    // Functions to be implemented in this class.
    NS_IMETHOD_(PRBool) IsDefaultBrowser();
    NS_IMETHOD_(int)    DisplayDialog();

  private:
    // Constructor (private; use nsIDefaultBrowser::GetInterface).
    nsWinDefaultBrowser();

    // Destructor clears singleton pointer.
    virtual ~nsWinDefaultBrowser() {
        pSingleton = NULL;
    }

    friend class nsIDefaultBrowser;
};

// Standard implementation of AddRef/Release/QueryInterface.
NS_IMPL_ISUPPORTS( nsWinDefaultBrowser, NS_IDEFAULTBROWSER_IID );

// Minimal ctor.
nsWinDefaultBrowser::nsWinDefaultBrowser() {
    NS_INIT_REFCNT();
}

/* nsWinDefaultBrowser::IsDefaultBrowser
 *
 * For now, use old logic.
 */
PRBool nsWinDefaultBrowser::IsDefaultBrowser() {
    PRBool result = !theApp.m_OwnedAndLostList.NonemptyLostIgnoredIntersection();

    return result;
}

/* nsWinDefaultBrowser::DisplayDialog
 *
 * Display dialog and return the user response.
 */
int nsWinDefaultBrowser::DisplayDialog() {
    CDefaultBrowserDlg dialog;
    int result = dialog.DoModal();
    return result;
}


nsIDefaultBrowser *nsIDefaultBrowser::GetInterface() {
    // If singleton hasn't been created, do it now.
    if ( !pSingleton ) {
        pSingleton = new nsWinDefaultBrowser;
    }
    pSingleton->AddRef();
    return pSingleton;
}
