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
#ifndef _NumEdit_h
#define _NumEdit_h

//NumEdit.h : header file


class CNSNumEdit: public CEdit
{
public:
		CNSNumEdit();
		virtual ~CNSNumEdit();
		int SetValue(int nNewValue);
		int GetValue();

protected:
        BOOL PreTranslateMessage( MSG* pMsg );

        afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
        afx_msg void OnSetFocus(CWnd* pOldWnd);
        DECLARE_MESSAGE_MAP()

};

#endif


