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

#include "findrepl.h"

CNetscapeFindReplaceDialog::CNetscapeFindReplaceDialog()	{
	m_pOwner = NULL;
	m_pSearchContext = NULL;

	// Don't show the help button
	m_fr.Flags &= ~FR_SHOWHELP;
}

void CNetscapeFindReplaceDialog::SetFrameGlue(CFrameGlue *pFrame)	{
	m_pOwner = pFrame;

	//	Let them know about us.
	m_pOwner->SetFindReplace(this);
}

void CNetscapeFindReplaceDialog::PostNcDestroy()	{
	//	Let the frame know we're gone.
	if(m_pOwner != NULL)	{
		m_pOwner->ClearFindReplace();
	}

	//	Call the base.
	CFindReplaceDialog::PostNcDestroy();
}

void CNetscapeFindReplaceDialog::SetSearchContext(MWContext *pSearchContext) {
    //  Allow this dialog to search a single frame only if desired.
    m_pSearchContext = pSearchContext;
}

MWContext * CNetscapeFindReplaceDialog::GetSearchContext() {
    if (m_pSearchContext)
	return m_pSearchContext;
    else 
	return NULL;
}
