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

/* edtrccln.h edtor client wrapper */
/* Dynamic Library wrapper for loading on call*/

#include "stdafx.h"
#include "edtrccln.h"

#ifdef WIN32
#define EDITORDLL "EDITOR32.DLL"
#else
#define EDITORDLL "EDITOR16.DLL"
#endif

HINSTANCE CEditorResourceDll::s_dllinstance=NULL;
unsigned int CEditorResourceDll::s_refcount=0;
IMAGEDIALOGCONSTRUCTOR CEditorResourceDll::s_pimagedialog=NULL;
IJPEGDLGCONSTRUCTOR CEditorResourceDll::s_pjpegdialog=NULL;
TAGDIALOGCONSTRUCTOR CEditorResourceDll::s_ptagdialog=NULL;
IMEDLLCONSTRUCTOR CEditorResourceDll::s_pimedll=NULL;

#ifdef XP_WIN32

#define PROC_IImageConversionDialog_Create "IImageConversionDialog_Create"
#define PROC_IIMEDll_Create                "IIMEDll_Create"
#define PROC_ITagDialog_Create             "ITagDialog_Create"
#define PROC_IJPEGOptionsDlg_Create        "IJPEGOptionsDlg_Create"

#else //XP_WIN16
#define PROC_IImageConversionDialog_Create "IIMAGECONVERSIONDIALOG_CREATE"
#define PROC_IIMEDll_Create                "IIMEDIALOG_CREATE"
#define PROC_ITagDialog_Create             "ITAGDIALOG_CREATE"
#define PROC_IJPEGOptionsDlg_Create        "IJPEGOPTIONSDLG_CREATE"
#endif

CEditorResourceDll::CEditorResourceDll()
{
    // Running under Win3.1, if the default drive is A or B and the disk
    //  was removed, this tries to load from the default drive.
    //  So change to C before loading. (A=1, B=2, C=3)
    if( _getdrive() < 3 ){
        _chdrive(3);
    }
    if (s_refcount)
    {
        s_dllinstance=LoadLibrary(EDITORDLL);
        s_refcount++;
        return;
    }
    else
    {
        s_dllinstance=LoadLibrary(EDITORDLL);
        XP_ASSERT(s_dllinstance);
        s_refcount++;
        if(!s_dllinstance)
            return;
        //retrieve all proc addresses and place them into holders
        s_pimagedialog= (IMAGEDIALOGCONSTRUCTOR)GetProcAddress(s_dllinstance,PROC_IImageConversionDialog_Create);
        XP_ASSERT(s_pimagedialog);

        s_ptagdialog= (TAGDIALOGCONSTRUCTOR)GetProcAddress(s_dllinstance,PROC_ITagDialog_Create);
        XP_ASSERT(s_ptagdialog);

        s_pimedll= (IMEDLLCONSTRUCTOR)GetProcAddress(s_dllinstance,PROC_IIMEDll_Create);
        XP_ASSERT(s_pimedll);

        s_pjpegdialog=(IJPEGDLGCONSTRUCTOR)GetProcAddress(s_dllinstance,PROC_IJPEGOptionsDlg_Create);
        XP_ASSERT(s_pjpegdialog);
    }
}

CEditorResourceDll::~CEditorResourceDll()
{
#ifdef WIN32
    if (!FreeLibrary(s_dllinstance))
        XP_ASSERT(0);
#else
    FreeLibrary(s_dllinstance);
#endif
       s_refcount--;
}

HINSTANCE
CEditorResourceDll::switchResources()
{
    XP_ASSERT(s_refcount);
    HINSTANCE t_hinstance=AfxGetResourceHandle();
    AfxSetResourceHandle(s_dllinstance);
    return t_hinstance;
}



IImageConversionDialog *
CEditorResourceDll::CreateImageConversionDialog(HWND pParent/*=NULL*/)
{
    XP_ASSERT(s_pimagedialog);
    if (!s_pimagedialog)
        return NULL;
    return (*s_pimagedialog)(pParent);
}

ITagDialog *
CEditorResourceDll::CreateTagDialog(IWFEInterface * wfe, IEDTInterface * edt, HWND hwnd,char * pchar)
{
    XP_ASSERT(s_ptagdialog);
    if (!s_ptagdialog)
        return NULL;
    return (*s_ptagdialog)(wfe,edt,pchar,hwnd);
}


IIMEDll *
CEditorResourceDll::CreateImeDll()
{
    XP_ASSERT(s_pimedll);
    if (!s_pimedll)
        return NULL;
    return (*s_pimedll)();
}

IJPEGOptionsDlg *
CEditorResourceDll::CreateJpegDialog(HWND pParent/*=NULL*/)
{
    XP_ASSERT(s_pjpegdialog);
    if (!s_pjpegdialog)
        return NULL;
    return (*s_pjpegdialog)(pParent);
}


