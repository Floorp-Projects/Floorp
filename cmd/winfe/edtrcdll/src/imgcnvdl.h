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

/*imgcnvdl.h*/
/*new dialog box for image conversion using Troy Chevalier's dialog box class*/
#include "edtiface.h"

class CImageConversionDialog : public CDialog ,public IImageConversionDialog
{
public:
    virtual int LOADDS DoModal();
    CImageConversionDialog(HWND pParent = NULL);   // standard constructor
private:
    IWFEInterface *m_wfeiface;
    HWND m_parent;
// Dialog Data
	enum { IDD = IDD_FECONVERTIMAGE };
	CString	m_outfilevalue;
    int m_listboxindex;
/* necessary overrides */
	virtual BOOL	DoTransfer(BOOL bSaveAndValidate);
    virtual BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);
/*elected overrides*/
    virtual BOOL InitDialog();

/*called by OnCommand*/
    void OnOK();
	void OnCancel();
private://methods
    void setListBoxChange(int p_index);//update the text and the .ext on the filename
    BOOL checkLock(){return m_filenamelock;}
    void attachExtention(CString &p_string,const CString &p_ext);
private://members set internally only
    BOOL m_filenamelock;
    CString m_oldstring; //used for comparison to see if variable changed
private://members set from the outside
	CString m_Doutfilename1;
	CString m_Dinputimagetype;
	DWORD m_Dinputimagesize;

    CONVERTOPTION *m_Doptionarray;
    DWORD m_Doptionarraycount;
    DWORD m_Doutputimagetype;//index into the above array
public:
    void LOADDS setConvertOptions(CONVERTOPTION *p_array,DWORD p_count){m_Doptionarray=p_array;m_Doptionarraycount=p_count;}
    void LOADDS setOutputFileName1(const char *p_string);

    void LOADDS setOutputImageType1(DWORD p_dword){m_Doutputimagetype=p_dword;}
    const char * LOADDS getOutputFileName1(){return m_Doutfilename1;}
	DWORD LOADDS getOutputImageType1(){return m_Doutputimagetype;}

    void LOADDS setWFEInterface(IWFEInterface *iface){m_wfeiface=iface;}
};


class CJpegOptionsDialog : public CDialog ,public IJPEGOptionsDlg
{
public:
    virtual int LOADDS DoModal();
    CJpegOptionsDialog(HWND pParent = NULL);   // standard constructor
private:
    HWND m_parent;
// Dialog Data
	enum { IDD = IDD_FECONVERTIMAGE };
    int m_outputquality;
/* necessary overrides */
	virtual BOOL	DoTransfer(BOOL bSaveAndValidate);
    virtual BOOL	OnCommand(int id, HWND hwndCtl, UINT notifyCode);
/*elected overrides*/
    virtual BOOL InitDialog();
    virtual void OnOK();
	virtual void OnCancel();
private:
    int m_Doutputquality;
public:
	void LOADDS setOutputImageQuality(DWORD p_short){m_Doutputquality=p_short;}
	DWORD LOADDS getOutputImageQuality(){return m_Doutputquality;}
};
