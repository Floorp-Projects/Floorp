/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#ifndef DROPTARGET_H
#define DROPTARGET_H

#include "MozillaBrowser.h"

// Simple drop target implementation

class CDropTarget : public CComObjectRootEx<CComSingleThreadModel>,
                    public IDropTarget
{
public:
    CDropTarget();
    virtual ~CDropTarget();

BEGIN_COM_MAP(CDropTarget)
    COM_INTERFACE_ENTRY(IDropTarget)
END_COM_MAP()

    // Data object currently being dragged
    CIPtr(IDataObject) m_spDataObject;

    // Owner of this object
    CMozillaBrowser *m_pOwner;

    // Sets the owner of this object
    void SetOwner(CMozillaBrowser *pOwner);
    // Helper method to extract a URL from an internet shortcut file
    HRESULT CDropTarget::GetURLFromFile(const TCHAR *pszFile, tstring &szURL);

// IDropTarget
    virtual HRESULT STDMETHODCALLTYPE DragEnter(/* [unique][in] */ IDataObject __RPC_FAR *pDataObj, /* [in] */ DWORD grfKeyState, /* [in] */ POINTL pt, /* [out][in] */ DWORD __RPC_FAR *pdwEffect);
    virtual HRESULT STDMETHODCALLTYPE DragOver(/* [in] */ DWORD grfKeyState, /* [in] */ POINTL pt, /* [out][in] */ DWORD __RPC_FAR *pdwEffect);
    virtual HRESULT STDMETHODCALLTYPE DragLeave(void);
    virtual HRESULT STDMETHODCALLTYPE Drop(/* [unique][in] */ IDataObject __RPC_FAR *pDataObj, /* [in] */ DWORD grfKeyState, /* [in] */ POINTL pt, /* [out][in] */ DWORD __RPC_FAR *pdwEffect);
};

typedef CComObject<CDropTarget> CDropTargetInstance;

#endif