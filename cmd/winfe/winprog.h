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

#include "widgetry.h"
#include "pw_public.h"

class CXPProgressDialog: public CDialog
{
public:
	CXPProgressDialog(CWnd *pParent =NULL);

	void SetCancelCallback(PW_CancelCallback cb, void*closure);
	void SetProgressValue(int32 value);
	int SetRange(int32 min,int32 max);
	virtual  BOOL OnInitDialog( );
	BOOL PreTranslateMessage( MSG* pMsg );
    CProgressMeter m_ProgressMeter;
	CStatic	m_PercentComplete;
	int32 m_Min;
	int32 m_Max;
	int32 m_Range;
	PW_CancelCallback m_cancelCallback;
	void * m_cancelClosure;

protected:
	virtual void OnCancel();
	virtual void DoDataExchange(CDataExchange*);
	afx_msg int OnCreate( LPCREATESTRUCT );

	DECLARE_MESSAGE_MAP()
};

extern CWnd *FE_GetDialogOwnerFromContext(MWContext *context);
