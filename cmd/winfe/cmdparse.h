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
//command line parser class
//takes a command line and parses out arguments separated by spaces 
//and whole arguments with spaces surrounded by quotes.

enum States {e_start, e_quote,e_end_quote, e_space, e_other, e_error, e_end};    



class CCmdParse
{   
 	void AddNewToken();   
 	void AddChar();
public:
	States State;
	States Previous;//currently not being used. may be later
		
	char m_szParseString[512];
	CStringList *m_pStringList;
	
	char *m_pCurrent;
	char m_Token[512];
	int m_nIndex;	                                                           
	                                                           
	CCmdParse(char *pszStringToParse,CStringList &lStringList);  //No need to call Init() if used

	CCmdParse(); //must call Init(...,...) with this constructor prior to calling ProcessCmdLine()
	~CCmdParse();
    BOOL ProcessCmdLine();
	void Init(char *pszStringToParse,CStringList &lStringList);

};
