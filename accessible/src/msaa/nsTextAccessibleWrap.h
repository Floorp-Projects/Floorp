/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Corp.
 * Portions created by Netscape Corp.are Copyright (C) 2003 Netscape
 * Corp. All Rights Reserved.
 *
 * Original Author: Aaron Leventhal
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _nsTextAccessibleWrap_H_
#define _nsTextAccessibleWrap_H_

#include "nsTextAccessible.h"
#include "ISimpleDOMText.h"
#include "nsRect.h"

class nsIFrame;
class nsPresContext;
class nsIRenderingContext;

class nsTextAccessibleWrap : public nsTextAccessible, 
                             public ISimpleDOMText
{
  public:
    nsTextAccessibleWrap(nsIDOMNode *, nsIWeakReference* aShell);
    virtual ~nsTextAccessibleWrap() {}

    // IUnknown methods - see iunknown.h for documentation
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
    STDMETHODIMP QueryInterface(REFIID, void**);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_domText( 
        /* [retval][out] */ BSTR __RPC_FAR *domText);
    
    virtual HRESULT STDMETHODCALLTYPE get_clippedSubstringBounds( 
        /* [in] */ unsigned int startIndex,
        /* [in] */ unsigned int endIndex,
        /* [out] */ int __RPC_FAR *x,
        /* [out] */ int __RPC_FAR *y,
        /* [out] */ int __RPC_FAR *width,
        /* [out] */ int __RPC_FAR *height);
    
    virtual HRESULT STDMETHODCALLTYPE get_unclippedSubstringBounds( 
        /* [in] */ unsigned int startIndex,
        /* [in] */ unsigned int endIndex,
        /* [out] */ int __RPC_FAR *x,
        /* [out] */ int __RPC_FAR *y,
        /* [out] */ int __RPC_FAR *width,
        /* [out] */ int __RPC_FAR *height);
    
    virtual HRESULT STDMETHODCALLTYPE scrollToSubstring( 
        /* [in] */ unsigned int startIndex,
        /* [in] */ unsigned int endIndex);

    virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_fontFamily( 
        /* [retval][out] */ BSTR __RPC_FAR *fontFamily);
    
  protected:
    nsresult GetCharacterExtents(PRInt32 aStartOffset, PRInt32 aEndOffset,
                                 PRInt32* aX, PRInt32* aY, 
                                 PRInt32* aWidth, PRInt32* aHeight);

    // Return child frame containing offset on success
    nsIFrame* GetPointFromOffset(nsIFrame *aContainingFrame, nsPresContext *aPresContext,
                                 nsIRenderingContext *aRenderingContext,
                                 PRInt32 aOffset, nsPoint& aOutPoint);
};

#endif

