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

#ifndef _EDTDLGS_H
#define _EDTDLGS_H 1
#include "edtiface.h"//for the callbacks
#ifndef IMEPRO
#ifdef XP_WIN16
#include "winnls16.h"
#endif //XP_WIN16
#endif //IMEPRO

#ifdef XP_WIN16
#define LOADDS __loadds
#else
#define LOADDS
#endif//XP_WIN16


struct CONVERTOPTION
{
    char *m_pencodername;
    char *m_pfileextention;
    char *m_phelpstring;
    XP_Bool m_builtin;
};

rc_interface IImageConversionDialog
{
public:
    virtual ~IImageConversionDialog(){}
    virtual int LOADDS DoModal()=0;
    virtual void LOADDS setConvertOptions(CONVERTOPTION *p_array,DWORD p_count)=0;

	virtual void LOADDS setOutputFileName1(const char *p_string)=0;
	virtual void LOADDS setOutputImageType1(DWORD p_int)=0;

    virtual const char * LOADDS getOutputFileName1()=0;
	virtual DWORD LOADDS getOutputImageType1()=0;

    virtual void  LOADDS setWFEInterface(IWFEInterface *)=0;
};

rc_interface IJPEGOptionsDlg
{
public:
    virtual ~IJPEGOptionsDlg(){}
    virtual int LOADDS DoModal()=0;
	virtual void LOADDS setOutputImageQuality(DWORD p_int)=0;
	virtual DWORD LOADDS getOutputImageQuality()=0;
};

rc_interface ITagDialog
{
public:
    virtual ~ITagDialog(){};
    virtual int LOADDS DoModal()=0;
};

rc_interface IIMEDll
{
public:                                                                                 
    virtual ~IIMEDll(){};
#ifdef WIN32
    virtual BOOL    ImmNotifyIME(HIMC,DWORD,DWORD,DWORD)=0;
    virtual HIMC    ImmGetContext(HWND)=0;
    virtual BOOL    ImmReleaseContext(HWND,HIMC)=0;
    virtual LONG    ImmGetCompositionString(HIMC,DWORD,LPVOID,DWORD)=0;
    virtual BOOL    ImmSetCompositionString(HIMC,DWORD,LPVOID,DWORD,LPVOID,DWORD)=0;
    virtual BOOL    ImmGetConversionStatus(HIMC,LPDWORD,LPDWORD)=0;
    virtual BOOL    ImmSetConversionStatus(HIMC,DWORD,DWORD)=0;
    virtual BOOL    ImmSetCandidateWindow(HIMC, LPCANDIDATEFORM)=0;
    virtual BOOL    ImmSetCompositionWindow(HIMC,LPCOMPOSITIONFORM)=0;
    virtual BOOL    ImmSetCompositionFont(HIMC,LPLOGFONT)=0;
    virtual LRESULT ImeEscape(HKL hKL,HIMC hIMC,UINT uEscape,LPVOID lpData)=0;
#else//WIN16
    virtual WORD    LOADDS SendIMEMessage( HWND, LPARAM )=0;
    virtual LRESULT LOADDS SendIMEMessageEx( HWND, LPARAM )=0; /* New for win3.1 */
    virtual BOOL    LOADDS IMPGetIME( HWND, LPIMEPRO )=0;
#endif//WIN32
};

IIMEDll *IIMEDll_Create();
IImageConversionDialog *IImageConversionDialog_Create(HWND pParent=NULL);
ITagDialog *ITagDialog_Create(IWFEInterface *wfe, IEDTInterface *edt, char *pTagData, HWND pParent);

#endif //_EDTDLGS_H

