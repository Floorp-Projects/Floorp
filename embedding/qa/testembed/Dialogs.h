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
 * Contributor(s): 
 *   Chak Nanga <chak@netscape.com> 
 */

#ifndef _DIALOGS_H_
#define _DIALOGS_H_

#include "resource.h"

class CBrowserView;

class CFindDialog : public CFindReplaceDialog	
{
public:
	CFindDialog(CString& csSearchStr, PRBool bMatchCase,
				PRBool bMatchWholeWord, PRBool bWrapAround,
				PRBool bSearchBackwards, CBrowserView* pOwner);
	BOOL WrapAround();
	BOOL SearchBackwards();

private:
	CString m_csSearchStr;
	PRBool m_bMatchCase;
	PRBool m_bMatchWholeWord;
	PRBool m_bWrapAround;
	PRBool m_bSearchBackwards;
	CBrowserView* m_pOwner;

protected:
	virtual BOOL OnInitDialog();
	virtual void PostNcDestroy();
};

#endif //_DIALOG_H_
