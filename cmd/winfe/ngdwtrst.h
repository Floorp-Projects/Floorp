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

#ifndef __TRUST_H
//	Avoid include redundancy
//
#define __TRUST_H

//	Purpose:    Contain a database of trusted/nontrusted applications which we will
//                  either spawn without user decision, or never spawn at all.
//	Comments:   Created to basically inform the user of potential security risks to their
//                  local machine.  This is not a solution, but is a stop gap measure.
//	Revision History:
//      04-10-95    created GAB
//

//	Required Includes
//

//	Constants
//

//	Structures
//
class CTrust    {   //  A matter of feeling.
    CString m_csExeName;    //  The name of the application of which we have an opinion.

public:
    enum Relationship    {
        m_Stranger, //  Don't talk to strangers.
        m_Friend    //  Geordi and Hue
    };
private:
    Relationship m_rFeeling;    //  How we feel about this app.

public:
    Relationship GetRelationship() {
        return m_rFeeling;
    }

	void SetRelationship(Relationship feeling) {
		m_rFeeling = feeling;
	}

    CString GetExeName()    {
        return m_csExeName;
    }

    CTrust(CString& csExeName, Relationship rFeeling)    {
        m_csExeName = csExeName;
        m_rFeeling = rFeeling;
    }
};

class CSpawnList    {   //  List of children we would like to have.
    CPtrList m_cplOpinions; //  List of apps which we have opinions on.

public:
    CSpawnList();   //  Create, read list from INI
    ~CSpawnList();  //  Destroy, write list to INI

	// Returns whether or not the application can be launched. If it isn't know whether
	// or not the application is trusted, a dialog is displayed asking the user what to do
    BOOL CanSpawn(CString& csExeName, CWnd *pWnd);

	// Returns TRUE if we should ask the user before downloading a file of this type or
	// FALSE if the user has indicated we trust it
	BOOL	PromptBeforeOpening(LPCSTR lpszExeName);

	// Sets whether we should prompt before opening a downloaded file of this type
	void	SetPromptBeforeOpening(LPCSTR lpszExeName, BOOL);
};

//	Global variables
//

//	Macros
//

//	Function declarations
//

#endif // __TRUST_H
