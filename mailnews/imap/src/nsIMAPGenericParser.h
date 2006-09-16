/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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

  // Add any specific stuff in the derived class
  virtual PRBool     LastCommandSuccessful();

  PRBool SyntaxError() { return (fParserState & stateSyntaxErrorFlag) != 0; }
  PRBool ContinueParse() { return fParserState == stateOK; }
  PRBool Connected() { return !(fParserState & stateDisconnectedFlag); }
  void SetConnected(PRBool error);
    
protected:

	// This is a pure virtual member which must be overridden in the derived class
	// for each different implementation of a nsIMAPGenericParser.
	// For instance, one implementation (the nsIMAPServerState) might get the next line
	// from an open socket, whereas another implementation might just get it from a buffer somewhere.
	// This fills in nextLine with the buffer, and returns PR_TRUE if everything is OK.
	// Returns PR_FALSE if there was some error encountered.  In that case, we reset the parser.
	virtual PRBool	GetNextLineForParser(char **nextLine) = 0;	

  virtual void	HandleMemoryFailure();
  void skip_to_CRLF();
  void skip_to_close_paren();
  char *CreateString();
  char *CreateAstring();
  char *CreateNilString();
  char *CreateLiteral();
  char *CreateAtom(PRBool isAstring = PR_FALSE);
  char *CreateQuoted(PRBool skipToEnd = PR_TRUE);
  char *CreateParenGroup();
  virtual void SetSyntaxError(PRBool error, const char *msg);

  void AdvanceToNextToken();
  void AdvanceToNextLine();
  void AdvanceTokenizerStartingPoint(int32 bytesToAdvance);
  void ResetLexAnalyzer();

protected:
	// use with care
  const char     *fNextToken;
  char           *fCurrentLine;
	char					 *fLineOfTokens;
  char           *fStartOfLineOfTokens;
  char           *fCurrentTokenPlaceHolder;
  PRBool          fAtEndOfLine;

private:
  enum nsIMAPGenericParserState { stateOK = 0,
                                  stateSyntaxErrorFlag = 0x1,
                                  stateDisconnectedFlag = 0x2 };
  PRUint32 fParserState;
};

#endif
