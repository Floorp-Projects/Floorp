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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* 
nsIMAPGenericParser is the base parser class used by the server parser and body shell parser
*/ 

#ifndef nsIMAPGenericParser_H
#define nsIMAPGenericParser_H

#include "nsImapCore.h"

#define WHITESPACE " \015\012"     // token delimiter 


class nsIMAPGenericParser 
{

public:
	nsIMAPGenericParser();
	virtual ~nsIMAPGenericParser();

    // Connected() && !SyntaxError()
	// Add any specific stuff in the derived class
    virtual PRBool     LastCommandSuccessful();
    
    PRBool     SyntaxError();
    virtual PRBool     ContinueParse();
    
    // if we get disconnected, end the current url processing and report to the
    // the user.
    PRBool			    Connected();
    virtual void        SetConnected(PRBool error);
    
    char        *CreateSyntaxErrorLine();

	// used to be XP_STRTOK_R, but that's not defined anymore.
	static char *	Imapstrtok_r(char *s1, const char *s2, char **lasts);
protected:

	// This is a pure virtual member which must be overridden in the derived class
	// for each different implementation of a nsIMAPGenericParser.
	// For instance, one implementation (the nsIMAPServerState) might get the next line
	// from an open socket, whereas another implementation might just get it from a buffer somewhere.
	// This fills in nextLine with the buffer, and returns TRUE if everything is OK.
	// Returns FALSE if there was some error encountered.  In that case, we reset the parser.
	virtual PRBool	GetNextLineForParser(char **nextLine) = 0;	

    virtual void	HandleMemoryFailure();
    virtual void    skip_to_CRLF();
    virtual void    skip_to_close_paren();
	virtual char	*CreateString();
	virtual char	*CreateAstring();
	virtual char	*CreateNilString();
    virtual char    *CreateLiteral();
	virtual char	*CreateAtom();
    virtual char    *CreateQuoted(PRBool skipToEnd = TRUE);
	virtual char	*CreateParenGroup();
    virtual void        SetSyntaxError(PRBool error);
    virtual PRBool     at_end_of_line();

    char *GetNextToken();
    void AdvanceToNextLine();
	void AdvanceTokenizerStartingPoint (int32 bytesToAdvance);
    void ResetLexAnalyzer();

protected:
	// use with care
    char                     *fNextToken;
    char                     *fCurrentLine;
	char					 *fLineOfTokens;
    char                     *fStartOfLineOfTokens;
    char                     *fCurrentTokenPlaceHolder;
    PRBool                   fAtEndOfLine;
	PRBool					  fTokenizerAdvanced;

private:

    char                     *fSyntaxErrorLine;
    PRBool                   fDisconnected;
    PRBool                   fSyntaxError;

};

#endif
