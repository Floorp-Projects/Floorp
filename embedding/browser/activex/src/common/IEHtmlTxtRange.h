/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexandre Trémon <atremon@elansoftware.com>
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
#ifndef IEHTMLTXTRANGE_H
#define IEHTMLTXTRANGE_H

#include "nsIDOMRange.h"

class CRange :
    public CComObjectRootEx<CComMultiThreadModel>
{
public:
    CRange();

protected:
    virtual ~CRange();

public:
    void SetRange(nsIDOMRange *pRange);

    virtual HRESULT GetParentElement(IHTMLElement **ppParent);

protected:
    nsCOMPtr<nsIDOMRange> mRange;
};

class CIEHtmlTxtRange :
    public CRange,
    public IDispatchImpl<IHTMLTxtRange, &IID_IHTMLTxtRange, &LIBID_MSHTML>
{
public:
    CIEHtmlTxtRange();

protected:
    virtual ~CIEHtmlTxtRange();

public:

BEGIN_COM_MAP(CIEHtmlTxtRange)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IHTMLTxtRange)
END_COM_MAP()


    // Implementation of IHTMLTxtRange
    virtual HRESULT STDMETHODCALLTYPE pasteHTML(BSTR html);
    virtual HRESULT STDMETHODCALLTYPE get_text(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE parentElement(IHTMLElement __RPC_FAR *__RPC_FAR *Parent);
    // Not yet implemented
    virtual HRESULT STDMETHODCALLTYPE get_htmlText(BSTR __RPC_FAR *p);
    virtual HRESULT STDMETHODCALLTYPE put_text(BSTR v);
    virtual HRESULT STDMETHODCALLTYPE duplicate(IHTMLTxtRange __RPC_FAR *__RPC_FAR *Duplicate);
    virtual HRESULT STDMETHODCALLTYPE inRange(IHTMLTxtRange __RPC_FAR *Range, VARIANT_BOOL __RPC_FAR *InRange);
    virtual HRESULT STDMETHODCALLTYPE isEqual(IHTMLTxtRange __RPC_FAR *Range, VARIANT_BOOL __RPC_FAR *IsEqual);
    virtual HRESULT STDMETHODCALLTYPE scrollIntoView(VARIANT_BOOL fStart = -1);
    virtual HRESULT STDMETHODCALLTYPE collapse(VARIANT_BOOL Start = -1);
    virtual HRESULT STDMETHODCALLTYPE expand(BSTR Unit,VARIANT_BOOL __RPC_FAR *Success);
    virtual HRESULT STDMETHODCALLTYPE move(BSTR Unit, long Count, long __RPC_FAR *ActualCount);
    virtual HRESULT STDMETHODCALLTYPE moveStart(BSTR Unit, long Count, long __RPC_FAR *ActualCount);
    virtual HRESULT STDMETHODCALLTYPE moveEnd(BSTR Unit, long Count, long __RPC_FAR *ActualCount);
    virtual HRESULT STDMETHODCALLTYPE select( void);
    virtual HRESULT STDMETHODCALLTYPE moveToElementText(IHTMLElement __RPC_FAR *element);
    virtual HRESULT STDMETHODCALLTYPE setEndPoint(BSTR how, IHTMLTxtRange __RPC_FAR *SourceRange);
    virtual HRESULT STDMETHODCALLTYPE compareEndPoints(BSTR how, IHTMLTxtRange __RPC_FAR *SourceRange, long __RPC_FAR *ret);
    virtual HRESULT STDMETHODCALLTYPE findText(BSTR String, long count, long Flags, VARIANT_BOOL __RPC_FAR *Success);
    virtual HRESULT STDMETHODCALLTYPE moveToPoint(long x, long y);
    virtual HRESULT STDMETHODCALLTYPE getBookmark(BSTR __RPC_FAR *Boolmark);
    virtual HRESULT STDMETHODCALLTYPE moveToBookmark(BSTR Bookmark, VARIANT_BOOL __RPC_FAR *Success);
    virtual HRESULT STDMETHODCALLTYPE queryCommandSupported(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
    virtual HRESULT STDMETHODCALLTYPE queryCommandEnabled(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
    virtual HRESULT STDMETHODCALLTYPE queryCommandState(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
    virtual HRESULT STDMETHODCALLTYPE queryCommandIndeterm(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
    virtual HRESULT STDMETHODCALLTYPE queryCommandText(BSTR cmdID, BSTR __RPC_FAR *pcmdText);
    virtual HRESULT STDMETHODCALLTYPE queryCommandValue(BSTR cmdID, VARIANT __RPC_FAR *pcmdValue);
    virtual HRESULT STDMETHODCALLTYPE execCommand(BSTR cmdID, VARIANT_BOOL showUI, VARIANT value, VARIANT_BOOL __RPC_FAR *pfRet);
    virtual HRESULT STDMETHODCALLTYPE execCommandShowHelp(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
};

#define CIEHTMLTXTRANGE_INTERFACES \
    COM_INTERFACE_ENTRY_IID(IID_IDispatch, IHTMLTxtRange \
    COM_INTERFACE_ENTRY_IID(IID_IHTMLTxtRange, IHTMLTxtRange)

typedef CComObject<CIEHtmlTxtRange> CIEHtmlTxtRangeInstance;

class CIEHtmlControlRange :
    public CRange,
    public IDispatchImpl<IHTMLControlRange, &IID_IHTMLControlRange, &LIBID_MSHTML>
{
public:
    CIEHtmlControlRange();

protected:
    virtual ~CIEHtmlControlRange();

public:

BEGIN_COM_MAP(CIEHtmlControlRange)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IHTMLControlRange)
END_COM_MAP()

    // Implementation of IHTMLControlRange
    virtual HRESULT STDMETHODCALLTYPE commonParentElement(IHTMLElement __RPC_FAR *__RPC_FAR *parent);
    // Not yet implemented
    virtual HRESULT STDMETHODCALLTYPE select( void);
    virtual HRESULT STDMETHODCALLTYPE add(IHTMLControlElement __RPC_FAR *item);
    virtual HRESULT STDMETHODCALLTYPE remove(long index);
    virtual HRESULT STDMETHODCALLTYPE item(long index, IHTMLElement __RPC_FAR *__RPC_FAR *pdisp);
    virtual HRESULT STDMETHODCALLTYPE scrollIntoView(VARIANT varargStart);
    virtual HRESULT STDMETHODCALLTYPE queryCommandSupported(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
    virtual HRESULT STDMETHODCALLTYPE queryCommandEnabled(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
    virtual HRESULT STDMETHODCALLTYPE queryCommandState(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
    virtual HRESULT STDMETHODCALLTYPE queryCommandIndeterm(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
    virtual HRESULT STDMETHODCALLTYPE queryCommandText(BSTR cmdID, BSTR __RPC_FAR *pcmdText);
    virtual HRESULT STDMETHODCALLTYPE queryCommandValue(BSTR cmdID, VARIANT __RPC_FAR *pcmdValue);
    virtual HRESULT STDMETHODCALLTYPE execCommand(BSTR cmdID, VARIANT_BOOL showUI, VARIANT value, VARIANT_BOOL __RPC_FAR *pfRet);
    virtual HRESULT STDMETHODCALLTYPE execCommandShowHelp(BSTR cmdID, VARIANT_BOOL __RPC_FAR *pfRet);
    virtual HRESULT STDMETHODCALLTYPE get_length(long __RPC_FAR *p);
};

typedef CComObject<CIEHtmlControlRange> CIEHtmlControlRangeInstance;

#endif //IEHTMLTXTRANGE_H