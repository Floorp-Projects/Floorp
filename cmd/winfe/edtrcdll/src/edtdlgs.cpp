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

#include "stdafx.h"
#include "resource.h"
#include "platform.h"
#include "edtdlgs.h"

//export includes
#include "imgcnvdl.h"
#include "tagdlg.h"
#include "imewrap.h"
//end dialog includes

#ifdef WIN32 
#define EXPORT 
#else
#define EXPORT __export
#endif

IImageConversionDialog * EXPORT
IImageConversionDialog_Create(HWND pParent)
{
    return new CImageConversionDialog(pParent);
}

ITagDialog * EXPORT
ITagDialog_Create(IWFEInterface *wfe, IEDTInterface *edt, char *pTagData, HWND pParent/*=NULL*/)
{
    CTagDlg *dlg=new CTagDlg(pParent,pTagData);
    dlg->setWFE(wfe);
    dlg->setEDT(edt);
    return dlg;
}

IIMEDll * EXPORT
IIMEDll_Create()
{
    return new CIMEDll();
}

IJPEGOptionsDlg * EXPORT
IJPEGOptionsDlg_Create(HWND pParent)
{
    return new CJpegOptionsDialog(pParent);
}
