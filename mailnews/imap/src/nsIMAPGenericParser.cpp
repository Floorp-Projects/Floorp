/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "msgCore.h"  // for pre-compiled headers

#include "nsImapCore.h"
#include "nsImapProtocol.h"
#include "nsIMAPGenericParser.h"
#include "nsString.h"
#include "nsReadableUtils.h"

/*************************************************
   The following functions are used to implement
   a thread safe strtok
 *************************************************/
/*
 * Get next token from string *stringp, where tokens are (possibly empty)
 * strings separated by characters from delim.  Tokens are separated
 * by exactly one delimiter iff the skip parameter is false; otherwise
 * they are separated by runs of characters from delim, because we
 * skip over any initial `delim' characters.
 *
 * Writes NULs into the string at *stringp to end tokens.
 * delim will usually, but need not, remain CONSTant from call to call.
 * On return, *stringp points past the last NUL written (if there might
 * be further tokens), or is NULL (if there are definitely no more tokens).
 *
 * If *stringp is NULL, strtoken returns NULL.
 */
static 
char *strtoken_r(char ** stringp, const char *delim, int skip)
{
	char *s;
	const char *spanp;
	int c, sc;
	char *tok;

	if ((s = *stringp) == NULL)
		return (NULL);

	if (skip) {
		/*
		 * Skip (span) leading delimiters (s += strspn(s, delim)).
		 */
	cont:
		c = *s;
		for (spanp = delim; (sc = *spanp++) != 0;) {
			if (c == sc) {
				s++;
				goto cont;
			}
		}
		if (c == 0) {		/* no token found */
			*stringp = NULL;
			return (NULL);
		}
	}

	/*
	 * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	 * Note that delim must have one NUL; we stop if we see that, too.
	 */
	for (tok = s;;) {
		c = *s++;
		spanp = delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*stringp = s;
				return( (char *) tok );
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
	return (NULL);
}


/* static */ char *nsIMAPGenericParser::Imapstrtok_r(char *s1, const char *s2, char **lasts)
{
	if (s1)
		*lasts = s1;
	return (strtoken_r(lasts, s2, 1));
}


////////////////// nsIMAPGenericParser /////////////////////////


nsIMAPGenericParser::nsIMAPGenericParser() :
	fNextToken(nsnull),
	fCurrentLine(nsnull),
	fLineOfTokens(nsnull),
  fStartOfLineOfTokens(nsnull),
	fCurrentTokenPlaceHolder(nsnull),
	fAtEndOfLine(PR_FALSE),
  fTokenizerAdvanced(PR_FALSE),
  fSyntaxErrorLine(nsnull),
	fSyntaxError(PR_FALSE),
	fDisconnected(PR_FALSE)
{
}

nsIMAPGenericParser::~nsIMAPGenericParser()
{
	PR_FREEIF( fCurrentLine );
	PR_FREEIF( fStartOfLineOfTokens);
	PR_FREEIF( fSyntaxErrorLine );
}

void nsIMAPGenericParser::HandleMemoryFailure()
{
	SetConnected(PR_FALSE);
}

void nsIMAPGenericParser::ResetLexAnalyzer()
{
	PR_FREEIF( fCurrentLine );
	PR_FREEIF( fStartOfLineOfTokens );
	fTokenizerAdvanced = PR_FALSE;
	
	fCurrentLine = fNextToken = fLineOfTokens = fStartOfLineOfTokens = fCurrentTokenPlaceHolder = nsnull;
	fAtEndOfLine = PR_FALSE;
}

PRBool nsIMAPGenericParser::LastCommandSuccessful()
{
	return Connected() && !SyntaxError();
}

void nsIMAPGenericParser::SetSyntaxError(PRBool error)
{
	fSyntaxError = error;
	PR_FREEIF( fSyntaxErrorLine );
	if (error)
	{
		NS_ASSERTION(PR_FALSE, "syntax error in generic parser");	
		fSyntaxErrorLine = PL_strdup(fCurrentLine);
	}
	else
		fSyntaxErrorLine = NULL;
}

char *nsIMAPGenericParser::CreateSyntaxErrorLine()
{
	return PL_strdup(fSyntaxErrorLine);
}


PRBool nsIMAPGenericParser::SyntaxError()
{
	return fSyntaxError;
}

void nsIMAPGenericParser::SetConnected(PRBool connected)
{
	fDisconnected = !connected;
}

PRBool nsIMAPGenericParser::Connected()
{
	return !fDisconnected;
}

PRBool nsIMAPGenericParser::ContinueParse()
{
	return !fSyntaxError && !fDisconnected;
}


PRBool nsIMAPGenericParser::at_end_of_line()
{
	return (fAtEndOfLine || (nsCRT::strcmp(fNextToken, CRLF) == 0));
}

void nsIMAPGenericParser::skip_to_CRLF()
{
	while (Connected() && !at_end_of_line())
		fNextToken = GetNextToken();
}

// fNextToken initially should point to
// a string after the initial open paren ("(")
// After this call, fNextToken points to the
// first character after the matching close
// paren.  Only call GetNextToken to get the NEXT
// token after the one returned in fNextToken.
void nsIMAPGenericParser::skip_to_close_paren()
{
	int numberOfCloseParensNeeded = 1;
	if (fNextToken && *fNextToken == ')')
	{
		numberOfCloseParensNeeded--;
		fNextToken++;
		if (!fNextToken || !*fNextToken)
			fNextToken = GetNextToken();
	}

	while (ContinueParse() && numberOfCloseParensNeeded > 0)
	{
		// go through fNextToken, count the number
		// of open and close parens, to account
		// for nested parens which might come in
		// the response
		char *loc = 0;
		for (loc = fNextToken; loc && *loc; loc++)
		{
			if (*loc == '(')
				numberOfCloseParensNeeded++;
			else if (*loc == ')')
				numberOfCloseParensNeeded--;
			if (numberOfCloseParensNeeded == 0)
			{
				fNextToken = loc + 1;
				if (!fNextToken || !*fNextToken)
					fNextToken = GetNextToken();
				break;	// exit the loop
			}
		}

		if (numberOfCloseParensNeeded > 0)
			fNextToken = GetNextToken();
	}
}

char *nsIMAPGenericParser::GetNextToken()
{
	if (!fCurrentLine || fAtEndOfLine)
		AdvanceToNextLine();
	else if (Connected())
	{
		if (fTokenizerAdvanced)
		{
			fNextToken = Imapstrtok_r(fLineOfTokens, WHITESPACE, &fCurrentTokenPlaceHolder);
			fTokenizerAdvanced = PR_FALSE;
		}
		else
		{
			fNextToken = Imapstrtok_r(nsnull, WHITESPACE, &fCurrentTokenPlaceHolder);
		}
		if (!fNextToken)
		{
			fAtEndOfLine = PR_TRUE;
			fNextToken = CRLF;
		}
	}
	
	return fNextToken;
}

void nsIMAPGenericParser::AdvanceToNextLine()
{
	PR_FREEIF( fCurrentLine );
	PR_FREEIF( fStartOfLineOfTokens);
	fTokenizerAdvanced = PR_FALSE;
	
	PRBool ok = GetNextLineForParser(&fCurrentLine);
	if (!ok)
	{
		SetConnected(PR_FALSE);
		fStartOfLineOfTokens = nsnull;
		fLineOfTokens = nsnull;
		fCurrentTokenPlaceHolder = nsnull;
		fNextToken = CRLF;
	}
	else if (fCurrentLine)	// might be NULL if we are would_block ?
	{
		fStartOfLineOfTokens = PL_strdup(fCurrentLine);
		if (fStartOfLineOfTokens)
		{
			fLineOfTokens = fStartOfLineOfTokens;
			fNextToken = Imapstrtok_r(fLineOfTokens, WHITESPACE, &fCurrentTokenPlaceHolder);
			if (!fNextToken)
			{
				fAtEndOfLine = PR_TRUE;
				fNextToken = CRLF;
			}
			else
				fAtEndOfLine = PR_FALSE;
		}
		else
			HandleMemoryFailure();
	}
	else
		HandleMemoryFailure();
}

void nsIMAPGenericParser::AdvanceTokenizerStartingPoint(int32 bytesToAdvance)
{
	PRInt32 startingDiff = fLineOfTokens - fStartOfLineOfTokens;
  PRInt32 nextTokenOffset;

  // save off offset into fStartOfLineOfTokens of fNextToken so we can set it appropriately
  // when we destroy the current line and create a new one. I'm pretty sure fNextToken must
  // point somewhere in the current line.
  nextTokenOffset = fNextToken - fStartOfLineOfTokens;

	PR_FREEIF(fStartOfLineOfTokens);
	if (fCurrentLine)
	{
		fStartOfLineOfTokens = PL_strdup(fCurrentLine);
    fNextToken = fStartOfLineOfTokens + nextTokenOffset;

		if (fStartOfLineOfTokens && ((int32) strlen(fStartOfLineOfTokens) >= bytesToAdvance))
		{
			fLineOfTokens = fStartOfLineOfTokens + bytesToAdvance  + startingDiff;
			fCurrentTokenPlaceHolder = fLineOfTokens;
			fTokenizerAdvanced = PR_TRUE;
		}
		else
			HandleMemoryFailure();
	}
	else
		HandleMemoryFailure();
}

// Lots of things in the IMAP protocol are defined as an "astring."
// An astring is either an atom or a string.
// An atom is just a series of one or more characters such as:  hello
// A string can either be quoted or literal.
// Quoted:  "Test Folder 1"
// Literal: {13}Test Folder 1
// This function leaves us off with fCurrentTokenPlaceHolder immediately after
// the end of the Astring.  Call GetNextToken() to get the token after it.
char *nsIMAPGenericParser::CreateAstring()
{
	if (*fNextToken == '{')
	{
		return CreateLiteral();		// literal
	}
	else if (*fNextToken == '"')
	{
		return CreateQuoted();		// quoted
	}
	else
	{
		return CreateAtom();		// atom
	}
}


// Create an atom
// This function does not advance the parser.
// Call GetNextToken() to get the next token after the atom.
char *nsIMAPGenericParser::CreateAtom()
{
	char *rv = PL_strdup(fNextToken);
	//fNextToken = GetNextToken();
	return (rv);
}

// CreateNilString creates either NIL (reutrns NULL) or a string
// Call with fNextToken pointing to the thing which we think is the nilstring.
// This function leaves us off with fCurrentTokenPlaceHolder immediately after
// the end of the string, if it is a string, or at the NIL.
// Regardless of type, call GetNextToken() to get the token after it.
char *nsIMAPGenericParser::CreateNilString()
{
	if (!PL_strncasecmp(fNextToken, "NIL", 3))
	{
		if (nsCRT::strlen(fNextToken) != 3)
			fNextToken += 3;
		//fNextToken = GetNextToken();
		return NULL;
	}
	else
		return CreateString();
}


// Create a string, which can either be quoted or literal,
// but not an atom.
// This function leaves us off with fCurrentTokenPlaceHolder immediately after
// the end of the String.  Call GetNextToken() to get the token after it.
char *nsIMAPGenericParser::CreateString()
{
	if (*fNextToken == '{')
	{
		char *rv = CreateLiteral();		// literal
		return (rv);
	}
	else if (*fNextToken == '"')
	{
		char *rv = CreateQuoted();		// quoted
		//fNextToken = GetNextToken();
		return (rv);
	}
	else
	{
		SetSyntaxError(PR_TRUE);
		return NULL;
	}
}


// This function leaves us off with fCurrentTokenPlaceHolder immediately after
// the end of the closing quote.  Call GetNextToken() to get the token after it.
// Note that if the current line ends without the
// closed quote then we have to fetch another line from the server, until
// we find the close quote.
char *nsIMAPGenericParser::CreateQuoted(PRBool /*skipToEnd*/)
{
	char *currentChar = fCurrentLine + 
					    (fNextToken - fStartOfLineOfTokens)
					    + 1;	// one char past opening '"'

	int  charIndex = 0;
	int  tokenIndex = 0;
	PRBool closeQuoteFound = PR_FALSE;
	nsCString returnString(currentChar);
	
	while (!closeQuoteFound && ContinueParse())
	{
		if (!returnString.CharAt(charIndex))
		{
			AdvanceToNextLine();
			returnString += fCurrentLine;
			charIndex++;
		}
		else if (returnString.CharAt(charIndex) == '"')
		{
			// don't check to see if it was escaped, 
			// that was handled in the next clause
			closeQuoteFound = PR_TRUE;
		}
		else if (returnString.CharAt(charIndex) == '\\')
		{
			// eat the escape character
			returnString.Cut(charIndex, 1);
			// whatever the escaped character was, we want it
			charIndex++;
			
			// account for charIndex not reflecting the eat of the escape character
			tokenIndex++;
		}
		else
			charIndex++;
	}
	
	if (closeQuoteFound)
	{
		returnString.SetCharAt(0, charIndex);
		//if ((charIndex == 0) && skipToEnd)	// it's an empty string.  Why skip to end?
		//	skip_to_CRLF();
		//else if (charIndex == strlen(fCurrentLine))	// should we have this?
		//AdvanceToNextLine();
		//else 
		if (charIndex < (int) (strlen(fNextToken) - 2))	// -2 because of the start and end quotes
		{
			// the quoted string was fully contained within fNextToken,
			// and there is text after the quote in fNextToken that we
			// still need
//			int charDiff = strlen(fNextToken) - charIndex - 1;
//			fCurrentTokenPlaceHolder -= charDiff;
//			if (!nsCRT::strcmp(fCurrentTokenPlaceHolder, CRLF))
//				fAtEndOfLine = PR_TRUE;
			AdvanceTokenizerStartingPoint ((fNextToken - fLineOfTokens) + nsCRT::strlen(returnString) + 2);
			if (!nsCRT::strcmp(fLineOfTokens, CRLF))
				fAtEndOfLine = PR_TRUE;
		}
		else
		{
			fCurrentTokenPlaceHolder += tokenIndex + charIndex + 1 - strlen(fNextToken);
			if (!*fCurrentTokenPlaceHolder)
				*fCurrentTokenPlaceHolder = ' ';	// put the token delimiter back
			/*	if (!nsCRT::strcmp(fNextToken, CRLF))
				fAtEndOfLine = PR_TRUE;
			*/
		}
	}
	else
		NS_ASSERTION(PR_FALSE, "didn't find close quote");
	
	return ToNewCString(returnString);
}


// This function leaves us off with fCurrentTokenPlaceHolder immediately after
// the end of the literal string.  Call GetNextToken() to get the token after it
// the literal string.
char *nsIMAPGenericParser::CreateLiteral()
{
	int32 numberOfCharsInMessage = atoi(fNextToken + 1);
	int32 charsReadSoFar = 0, currentLineLength = 0;
	int32 bytesToCopy = 0;
	
	char *returnString = (char *) PR_Malloc(numberOfCharsInMessage + 1);
	
	if (returnString)
	{
		*(returnString + numberOfCharsInMessage) = 0; // Null terminate it first

		PRBool terminatedLine = PR_FALSE;
		while (ContinueParse() && (charsReadSoFar < numberOfCharsInMessage))
		{
			if (!terminatedLine)
			{
				if (fCurrentTokenPlaceHolder &&
					*fCurrentTokenPlaceHolder == nsCRT::LF &&
					*(fCurrentTokenPlaceHolder+1))
				{
					// This is a static buffer, with a CRLF between the literal size ({91}) and
					// the string itself
					fCurrentTokenPlaceHolder++;
				}
				else
				{
					// We have to read the next line from AdvanceToNextLine().
					terminatedLine = PR_TRUE;
					AdvanceToNextLine();
				}
			}
			else
				AdvanceToNextLine();
			currentLineLength = strlen(terminatedLine ? fCurrentLine : fCurrentTokenPlaceHolder);
			bytesToCopy = (currentLineLength > numberOfCharsInMessage - charsReadSoFar ?
						   numberOfCharsInMessage - charsReadSoFar : currentLineLength);
			NS_ASSERTION (bytesToCopy, "0 length literal?");

			if (ContinueParse())
			{
				nsCRT::memcpy(returnString + charsReadSoFar, terminatedLine ? fCurrentLine : fCurrentTokenPlaceHolder, bytesToCopy); 
				charsReadSoFar += bytesToCopy;
			}
		}
		
		if (ContinueParse())
		{
			if (bytesToCopy == 0)
			{
				skip_to_CRLF();
				fAtEndOfLine = PR_TRUE;
			}
			else if (currentLineLength == bytesToCopy)
			{
				fAtEndOfLine = PR_TRUE;
			}
			else
			{
				// Move fCurrentTokenPlaceHolder
				if (terminatedLine)
					AdvanceTokenizerStartingPoint (bytesToCopy);
				else
					AdvanceTokenizerStartingPoint (	bytesToCopy + 
													strlen(fNextToken) + 
													2 /* CRLF */ +
													(fNextToken - fLineOfTokens)
													);
				if (!*fCurrentTokenPlaceHolder)	// landed on a token boundary
					fCurrentTokenPlaceHolder++;
				if (!nsCRT::strcmp(fCurrentTokenPlaceHolder, CRLF))
					fAtEndOfLine = PR_TRUE;
			}	
		}
	}

	return returnString;
}


// Call this to create a buffer containing all characters within
// a given set of parentheses.
// Call this with fNextToken[0]=='(', that is, the open paren
// of the group.
// It will allocate and return all characters up to and including the corresponding
// closing paren, and leave the parser in the right place afterwards.
char *nsIMAPGenericParser::CreateParenGroup()
{
#ifdef DEBUG_bienvenu
	NS_ASSERTION(fNextToken[0] == '(', "we don't have a paren group!");
#endif

	int numOpenParens = 1;

	// build up a buffer with the paren group.
	// start with an initial chunk, expand later if necessary
	nsCString buf;
	nsCString returnString;
	int bytesUsed = 0;
	
	// count the number of parens in the current token
	int count, tokenLen = strlen(fNextToken);
	for (count = 1; (count < tokenLen) && (numOpenParens > 0); count++)
	{
		if (fNextToken[count] == '(')
			numOpenParens++;
		else if (fNextToken[count] == ')')
			numOpenParens--;
	}

	if ((numOpenParens > 0) && ContinueParse())
	{
		// Copy that first token from before
		returnString =fNextToken;
		returnString.Append(" ");	// space that got stripped off the token

		PRBool extractReset = PR_TRUE;
		while (extractReset && ContinueParse())
		{
			extractReset = PR_FALSE;
			// Go through the current line and look for the last close paren.
			// We're not trying to parse it just yet, just separate it out.
			int len = strlen(fCurrentTokenPlaceHolder);
			for (count = 0; (count < len) && (numOpenParens > 0) && !extractReset; count++)
			{
				if (*fCurrentTokenPlaceHolder == '{')
				{
					fNextToken = GetNextToken();
					NS_ASSERTION(fNextToken, "out of memory?or invalid syntax");
					if (fNextToken)
					{
						tokenLen = strlen(fNextToken);
						if (fNextToken[tokenLen-1] == '}')
						{
							// ok, we're looking at a literal string here
						
							// first, flush buf
							if (bytesUsed > 0)
							{
								buf.Truncate(bytesUsed);
								returnString.Append(buf);
                buf.Truncate();
								bytesUsed = 0;
							}

							returnString.Append(fNextToken);	// append the {xx} to the buffer
							returnString.Append(CRLF);			// append a CRLF to the buffer
							char *lit = CreateLiteral();
							fTokenizerAdvanced = PR_FALSE;	// force it to use fCurrentTokenPlaceHolder
							NS_ASSERTION(lit, "syntax error or out of memory");
							if (lit)
							{
								returnString.Append(lit);
								//fCurrentTokenPlaceHolder += nsCRT::strlen(lit);
								//AdvanceTokenizerStartingPoint(nsCRT::strlen(lit));
								//fNextToken = GetNextToken();
								extractReset = PR_TRUE;
								PR_Free(lit);
							}
						}
						else
						{
#ifdef DEBUG_bienvenu
							NS_ASSERTION(PR_FALSE, "syntax error creating paren group");	// maybe not an error, but definitely a rare condition
#endif
						}
					}
				}
				else if (*fCurrentTokenPlaceHolder == '"')
				{
					// We're looking at a quoted string here.
					// Ignore the characters within it.

					// first, flush buf
					if (bytesUsed > 0)
					{
						buf.Truncate(bytesUsed);
						returnString.Append(buf);
            buf.Truncate();
						bytesUsed = 0;
					}

					fNextToken = GetNextToken();
					NS_ASSERTION(fNextToken, "syntax error or out of memory creating paren group");
					if (fNextToken)
					{
						char *q = CreateQuoted();
						fTokenizerAdvanced = PR_FALSE;	// force it to use fCurrentTokenPlaceHolder
						NS_ASSERTION(q, "syntax error or out of memory creating paren group");
						if (q)
						{
							returnString.Append("\"");
							returnString.Append(q);
							returnString.Append("\"");
							extractReset = PR_TRUE;
							PR_Free(q);
						}
					}
				}
				else if (*fCurrentTokenPlaceHolder == '(')
					numOpenParens++;
				else if (*fCurrentTokenPlaceHolder == ')')
					numOpenParens--;


				if (!extractReset)
				{
					// append this character to the buffer
					buf += *fCurrentTokenPlaceHolder;

					//.SetCharAt(*fCurrentTokenPlaceHolder, bytesUsed);
					bytesUsed++;
					fCurrentTokenPlaceHolder++;
				}
			}
		}
	}
	else if ((numOpenParens == 0) && ContinueParse())
	{
		// the whole paren group response was finished in a single token
		buf.Append(fNextToken);
	}


	if (numOpenParens != 0 || !ContinueParse())
	{
		SetSyntaxError(PR_TRUE);
		returnString.SetLength(0);
	}
	else
	{
		// flush buf the final time
		if (bytesUsed > 0)
		{
			buf.Truncate(bytesUsed);
			returnString.Append(buf);
      buf.Truncate();
		}
		fNextToken = GetNextToken();
	}

	return ToNewCString(returnString);
}

