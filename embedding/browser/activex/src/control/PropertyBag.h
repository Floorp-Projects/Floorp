/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Author:
 *   Adam Lock <adamlock@netscape.com>
 *
 * Contributor(s): 
 */
#ifndef PROPERTYBAG_H
#define PROPERTYBAG_H


// Object wrapper for property list. This class can be set up with a
// list of properties and used to initialise a control with them

class CPropertyBag :    public CComObjectRootEx<CComSingleThreadModel>,
                        public IPropertyBag
{
    // List of properties in the bag
    PropertyList m_PropertyList;

public:
    // Constructor
    CPropertyBag();
    // Destructor
    virtual ~CPropertyBag();

BEGIN_COM_MAP(CPropertyBag)
    COM_INTERFACE_ENTRY(IPropertyBag)
END_COM_MAP()

// IPropertyBag methods
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Read(/* [in] */ LPCOLESTR pszPropName, /* [out][in] */ VARIANT __RPC_FAR *pVar, /* [in] */ IErrorLog __RPC_FAR *pErrorLog);
    virtual HRESULT STDMETHODCALLTYPE Write(/* [in] */ LPCOLESTR pszPropName, /* [in] */ VARIANT __RPC_FAR *pVar);
};

typedef CComObject<CPropertyBag> CPropertyBagInstance;

#endif