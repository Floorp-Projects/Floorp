/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 *   Simon Fraser  <sfraser@netscape.com>
 *   Akkana Peck   <akkana@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsWebBrowserFindImpl_h__
#define nsWebBrowserFindImpl_h__

#include "nsIWebBrowserFind.h"

#include "nsCOMPtr.h"
#include "nsWeakReference.h"

#include "nsIFind.h"

#include "nsString.h"

#define NS_WEB_BROWSER_FIND_CONTRACTID "@mozilla.org/embedcomp/find;1"

#define NS_WEB_BROWSER_FIND_CID \
 {0x57cf9383, 0x3405, 0x11d5, {0xbe, 0x5b, 0xaa, 0x20, 0xfa, 0x2c, 0xf3, 0x7c}}

class nsISelection;
class nsIDOMWindow;

class nsIDocShell;

//*****************************************************************************
// class nsWebBrowserFind
//*****************************************************************************   

class nsWebBrowserFind  : public nsIWebBrowserFind,
                          public nsIWebBrowserFindInFrames
{
public:
                nsWebBrowserFind();
    virtual     ~nsWebBrowserFind();
    
    // nsISupports
    NS_DECL_ISUPPORTS
    
    // nsIWebBrowserFind
    NS_DECL_NSIWEBBROWSERFIND
                
    // nsIWebBrowserFindInFrames
    NS_DECL_NSIWEBBROWSERFINDINFRAMES


protected:
     
    bool        CanFindNext()
                { return mSearchString.Length() != 0; }

    nsresult    SearchInFrame(nsIDOMWindow* aWindow, bool aWrapping,
                              bool* didFind);

    nsresult    OnStartSearchFrame(nsIDOMWindow *aWindow);
    nsresult    OnEndSearchFrame(nsIDOMWindow *aWindow);

    void        GetFrameSelection(nsIDOMWindow* aWindow, nsISelection** aSel);
    nsresult    ClearFrameSelection(nsIDOMWindow *aWindow);
    
    nsresult    OnFind(nsIDOMWindow *aFoundWindow);
    
    nsIDocShell *GetDocShellFromWindow(nsIDOMWindow *inWindow);

    void        SetSelectionAndScroll(nsIDOMWindow* aWindow, 
                                      nsIDOMRange* aRange);

    nsresult    GetRootNode(nsIDOMDocument* aDomDoc, nsIDOMNode** aNode);
    nsresult    GetSearchLimits(nsIDOMRange* aRange,
                                nsIDOMRange* aStartPt,
                                nsIDOMRange* aEndPt,
                                nsIDOMDocument* aDoc,
                                nsISelection* aSel,
                                bool aWrap);
    nsresult    SetRangeAroundDocument(nsIDOMRange* aSearchRange,
                                       nsIDOMRange* aStartPoint,
                                       nsIDOMRange* aEndPoint,
                                       nsIDOMDocument* aDoc);
    
protected:

    nsString        mSearchString;
    
    bool            mFindBackwards;
    bool            mWrapFind;
    bool            mEntireWord;
    bool            mMatchCase;
    
    bool            mSearchSubFrames;
    bool            mSearchParentFrames;

    nsWeakPtr       mCurrentSearchFrame;    // who knows if windows can go away during our lifetime, hence weak
    nsWeakPtr       mRootSearchFrame;       // who knows if windows can go away during our lifetime, hence weak
    nsWeakPtr       mLastFocusedWindow;     // who knows if windows can go away during our lifetime, hence weak
    
    nsCOMPtr<nsIFind> mFind;
};

#endif
