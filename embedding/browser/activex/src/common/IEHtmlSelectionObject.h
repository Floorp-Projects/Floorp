/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 *   Alexandre Trémon <atremon@elansoftware.com>
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
#ifndef IEHTMLSELECTIONOBJECT_H
#define IEHTMLSELECTIONOBJECT_H

#include "nsISelection.h"
#include "nsIDOMDocumentRange.h"

class CIEHtmlSelectionObject :
    public IDispatchImpl<IHTMLSelectionObject, &IID_IHTMLSelectionObject, &LIBID_MSHTML>,
    public CComObjectRootEx<CComMultiThreadModel>
{
public:
    CIEHtmlSelectionObject();

protected:
    virtual ~CIEHtmlSelectionObject();

public:
    void SetSelection(nsISelection *pSelection);
    void SetDOMDocumentRange(nsIDOMDocumentRange *pDOMDocumentRange);

BEGIN_COM_MAP(CIEHtmlSelectionObject)
    COM_INTERFACE_ENTRY_IID(IID_IDispatch, IHTMLSelectionObject)
    COM_INTERFACE_ENTRY(IHTMLSelectionObject)
END_COM_MAP()


    // Implementation of IHTMLSelectionObject
    virtual HRESULT STDMETHODCALLTYPE createRange(IDispatch __RPC_FAR *__RPC_FAR *range);
    virtual HRESULT STDMETHODCALLTYPE empty();
    virtual HRESULT STDMETHODCALLTYPE clear();
    virtual HRESULT STDMETHODCALLTYPE get_type(BSTR __RPC_FAR *p);

protected:
    nsCOMPtr<nsISelection> mSelection;
    // IE does not return null range when selection is empty 
    // so this reference allows for creating an empty range :
    nsCOMPtr<nsIDOMDocumentRange> mDOMDocumentRange;
};

#define CIEHTMLSELECTIONOBJECT_INTERFACES \
    COM_INTERFACE_ENTRY_IID(IID_IDispatch, IHTMLSelectionObject \
    COM_INTERFACE_ENTRY_IID(IID_IHTMLSelectionObject, IHTMLSelectionObject)

typedef CComObject<CIEHtmlSelectionObject> CIEHtmlSelectionObjectInstance;

#endif //IEHTMLSELECTIONOBJECT_H