/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
	// This fills in nextLine with the buffer, and returns PR_TRUE if everything is OK.
	// Returns PR_FALSE if there was some error encountered.  In that case, we reset the parser.
	virtual PRBool	GetNextLineForParser(char **nextLine) = 0;	

  virtual void	HandleMemoryFailure();
  virtual void    skip_to_CRLF();
  virtual void    skip_to_close_paren();
	virtual char	*CreateString();
	virtual char	*CreateAstring();
	virtual char	*CreateNilString();
  virtual char    *CreateLiteral();
	virtual char	*CreateAtom();
  virtual char    *CreateQuoted(PRBool skipToEnd = PR_TRUE);
	virtual char	*CreateParenGroup();
  virtual void        SetSyntaxError(PRBool error);
  virtual PRBool     at_end_of_line();

  char *GetNextToken();
  void AdvanceToNextLine();
	void AdvanceTokenizerStartingPoint (int32 bytesToAdvance);
  void ResetLexAnalyzer();

protected:
	// use with care
  char           *fNextToken;
  char           *fCurrentLine;
	char					 *fLineOfTokens;
  char           *fStartOfLineOfTokens;
  char           *fCurrentTokenPlaceHolder;
  PRBool          fAtEndOfLine;
	PRBool				  fTokenizerAdvanced;

  char           *fSyntaxErrorLine;
  PRBool          fSyntaxError;
private:
  PRBool          fDisconnected;


};

#endif
