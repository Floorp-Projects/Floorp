/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Author:
 *   Conrad Carlen <ccarlen@netscape.com>
 */

#ifndef nsWebBrowserFindImpl_h__
#define nsWebBrowserFindImpl_h__

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIFindAndReplace.h"

class nsIDOMWindow;
class nsITextServicesDocument;

//*****************************************************************************
// class nsWebBrowserFindImpl
//*****************************************************************************   

class nsWebBrowserFindImpl
{
public:
                nsWebBrowserFindImpl();
                ~nsWebBrowserFindImpl();
                
    nsresult    Init(); // Must be called after constructor

    nsresult    SetSearchString(const PRUnichar* aString);
    nsresult    GetSearchString(PRUnichar** aString);

    PRBool      GetFindBackwards()
                { return mFindBackwards; }
    void        SetFindBackwards(PRBool aFindBackwards)
                { mFindBackwards = aFindBackwards; }
                
    PRBool      GetWrapFind()
                { return mWrapFind; }
    void        SetWrapFind(PRBool aWrapFind)
                { mWrapFind = aWrapFind; }

    PRBool      GetEntireWord()
                { return mEntireWord; }
    void        SetEntireWord(PRBool aEntireWord)
                { mEntireWord = aEntireWord; }

    PRBool      GetMatchCase()
                { return mMatchCase; }
    void        SetMatchCase(PRBool aMatchCase)
                { mMatchCase = aMatchCase; }
                
    PRBool      CanFindNext()
                { return mSearchString.Length() != 0; }

    nsresult    DoFind(nsIDOMWindow* aWindow, PRBool* didFind);

private:
    nsresult    MakeTSDocument(nsIDOMWindow* aWindow, nsITextServicesDocument** aDoc);

    nsString    mSearchString;
    PRBool      mFindBackwards, mWrapFind, mEntireWord, mMatchCase;
    
    nsCOMPtr<nsIFindAndReplace> mTSFind;
};

#endif
