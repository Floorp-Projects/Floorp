/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*edtrccln.h edtor client wrapper */
/*Dynamic Library wrapper for loading on call*/
#ifndef _EDTRCCLN_H

#define _EDTRCCLN_H
#include "edtrcdll\edtdlgs.h"

//typedefs of placeholders for exported functions
typedef IImageConversionDialog *(*IMAGEDIALOGCONSTRUCTOR)(HWND);
typedef IJPEGOptionsDlg *(*IJPEGDLGCONSTRUCTOR)(HWND);
typedef ITagDialog *(*TAGDIALOGCONSTRUCTOR)(IWFEInterface *, IEDTInterface *, char *,HWND);
typedef IIMEDll *(*IMEDLLCONSTRUCTOR)();

class CEditorResourceDll
{
    static HINSTANCE s_dllinstance;
    static unsigned int s_refcount;
    //placeholders for exported functions
    static IMAGEDIALOGCONSTRUCTOR s_pimagedialog;
    static IJPEGDLGCONSTRUCTOR s_pjpegdialog;
    static TAGDIALOGCONSTRUCTOR s_ptagdialog;
    static IMEDLLCONSTRUCTOR s_pimedll;

public:
    HINSTANCE switchResources();
    CEditorResourceDll();
    ~CEditorResourceDll();
    //wrappers for exported functions
    IImageConversionDialog *CreateImageConversionDialog(HWND pParent=NULL);
    ITagDialog *CreateTagDialog(IWFEInterface *, IEDTInterface *, HWND pParent=NULL, char *pchar=NULL);
    IIMEDll *CreateImeDll();
    IJPEGOptionsDlg *CreateJpegDialog(HWND pParent=NULL);
};

//thread safe resource handle switcher. also reference counts the dll. ya!
class CEditorResourceSwitcher:public CEditorResourceDll
{
    HINSTANCE m_oldresourcehandle;
public:
    CEditorResourceSwitcher(){m_oldresourcehandle=switchResources();}
    void Reset(){AfxSetResourceHandle(m_oldresourcehandle);}
    ~CEditorResourceSwitcher(){Reset();}
};

#endif //_EDTRCCLN_H

