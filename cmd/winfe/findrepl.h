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
 
#ifndef __FINDREPLACE_H
#define __FINDREPLACE_H

//	Need to derive a special find replace dialog
//		so we can handle the modeless nature
//		notification when closing down.

class CNetscapeFindReplaceDialog : public CFindReplaceDialog	{
private:
	CFrameGlue *m_pOwner;
	MWContext *m_pSearchContext;
public:
	CNetscapeFindReplaceDialog();
	void SetFrameGlue(CFrameGlue *pFrame);
	void SetSearchContext(MWContext *pSearchContext);
	MWContext* GetSearchContext() ;

protected:
	virtual void PostNcDestroy();
};

#endif // __FINDREPLACE_H
