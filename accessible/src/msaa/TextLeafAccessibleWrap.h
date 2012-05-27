/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TextLeafAccessibleWrap_h__
#define mozilla_a11y_TextLeafAccessibleWrap_h__

#include "TextLeafAccessible.h"
#include "ISimpleDOMText.h"
#include "nsRect.h"

class nsIFrame;
class nsRenderingContext;

namespace mozilla {
namespace a11y {
 
class TextLeafAccessibleWrap : public TextLeafAccessible, 
                               public ISimpleDOMText
{
public:
  TextLeafAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~TextLeafAccessibleWrap() {}

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
    nsIFrame* GetPointFromOffset(nsIFrame *aContainingFrame,
                                 PRInt32 aOffset, bool aPreferNext, nsPoint& aOutPoint);
};

} // namespace a11y
} // namespace mozilla

#endif

