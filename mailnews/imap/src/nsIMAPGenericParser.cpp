/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "msgCore.h"  // for pre-compiled headers

#include "nsImapCore.h"
#include "nsImapProtocol.h"
#include "nsIMAPGenericParser.h"
#include "nsString2.h"

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
}


/* static */ char *nsIMAPGenericParser::Imapstrtok_r(char *s1, const char *s2, char **lasts)
{
	if (s1)
		*lasts = s1;
	return (strtoken_r(lasts, s2, 1));
}


////////////////// nsIMAPGenericParser /////////////////////////


nsIMAPGenericParser::nsIMAPGenericParser() :
	fNextToken(nil),
	fCurrentLine(nil),
	fLineOfTokens(nil),
	fCurrentTokenPlaceHolder(nil),
	fStartOfLineOfTokens(nil),
	fSyntaxErrorLine(nil),
	fAtEndOfLine(PR_FALSE),
	fDisconnected(PR_FALSE),
	fSyntaxError(PR_FALSE),
	fTokenizerAdvanced(PR_FALSE)
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
	
	fCurrentLine = fNextToken = fLineOfTokens = fStartOfLineOfTokens = fCurrentTokenPlaceHolder = nil;
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
		if (!fSyntaxErrorLine)
		{
			HandleMemoryFailure();
//			PR_LOG(IMAP, out, ("PARSER: Internal Syntax Error: <no line>"));
		}
		else
		{
//			if (!nsCRT::strcmp(fSyntaxErrorLine, CRLF))
//				PR_LOG(IMAP, out, ("PARSER: Internal Syntax Error: <CRLF>"));
//			else
//				PR_LOG(IMAP, out, ("PARSER: Internal Syntax Error: %s", fSyntaxErrorLine));
		}
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
	return nsCRT::strcmp(fNextToken, CRLF) == 0;
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
			fNextToken = Imapstrtok_r(nil, WHITESPACE, &fCurrentTokenPlaceHolder);
		}
		if (!fNextToken)
		{
			fAtEndOfLine = TRUE;
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
		fStartOfLineOfTokens = nil;
		fLineOfTokens = nil;
		fCurrentTokenPlaceHolder = nil;
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
				fAtEndOfLine = TRUE;
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
	PR_FREEIF(fStartOfLineOfTokens);
	if (fCurrentLine)
	{
		fStartOfLineOfTokens = PL_strdup(fCurrentLine);
		if (fStartOfLineOfTokens && ((int32) PL_strlen(fStartOfLineOfTokens) >= bytesToAdvance))
		{
			fLineOfTokens = fStartOfLineOfTokens + bytesToAdvance;
			fTokenizerAdvanced = TRUE;
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
	if (!PL_strcasecmp(fNextToken, "NIL"))
	{
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
		SetSyntaxError(TRUE);
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
	nsString2 returnString = currentChar;
	
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
			closeQuoteFound = TRUE;
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
	
	if (closeQuoteFound && returnString)
	{
		returnString.SetCharAt(0, charIndex);
		//if ((charIndex == 0) && skipToEnd)	// it's an empty string.  Why skip to end?
		//	skip_to_CRLF();
		//else if (charIndex == PL_strlen(fCurrentLine))	// should we have this?
		//AdvanceToNextLine();
		//else 
		if (charIndex < (int) (PL_strlen(fNextToken) - 2))	// -2 because of the start and end quotes
		{
			// the quoted string was fully contained within fNextToken,
			// and there is text after the quote in fNextToken that we
			// still need
			int charDiff = PL_strlen(fNextToken) - charIndex - 1;
			fCurrentTokenPlaceHolder -= charDiff;
			if (!nsCRT::strcmp(fCurrentTokenPlaceHolder, CRLF))
				fAtEndOfLine = TRUE;
		}
		else
		{
			fCurrentTokenPlaceHolder += tokenIndex + charIndex + 2 - PL_strlen(fNextToken);
			if (!nsCRT::strcmp(fCurrentTokenPlaceHolder, CRLF))
				fAtEndOfLine = TRUE;
			/*
			tokenIndex += charIndex;
			fNextToken = currentChar + tokenIndex + 1;
			if (!nsCRT::strcmp(fNextToken, CRLF))
				fAtEndOfLine = TRUE;
			*/
		}
	}
	
	return returnString.ToNewCString();
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

		while (ContinueParse() && (charsReadSoFar < numberOfCharsInMessage))
		{
			AdvanceToNextLine();
			currentLineLength = PL_strlen(fCurrentLine);
			bytesToCopy = (currentLineLength > numberOfCharsInMessage - charsReadSoFar ?
						   numberOfCharsInMessage - charsReadSoFar : currentLineLength);
			NS_ASSERTION (bytesToCopy, "0 length literal?");

			if (ContinueParse())
			{
				nsCRT::memcpy(returnString + charsReadSoFar, fCurrentLine, bytesToCopy); 
				charsReadSoFar += bytesToCopy;
			}
		}
		
		if (ContinueParse())
		{
			if (bytesToCopy == 0)
			{
				skip_to_CRLF();
				fAtEndOfLine = TRUE;
				//fNextToken = GetNextToken();
			}
			else if (currentLineLength == bytesToCopy)
			{
				fAtEndOfLine = TRUE;
				//AdvanceToNextLine();
			}
			else
			{
				// Move fCurrentTokenPlaceHolder

				//fCurrentTokenPlaceHolder = fStartOfLineOfTokens + bytesToCopy;
				AdvanceTokenizerStartingPoint (bytesToCopy);
				if (!*fCurrentTokenPlaceHolder)	// landed on a token boundary
					fCurrentTokenPlaceHolder++;
				if (!nsCRT::strcmp(fCurrentTokenPlaceHolder, CRLF))
					fAtEndOfLine = TRUE;

				// The first token on the line might not
				// be at the beginning of the line.  There might be ONLY
				// whitespace before it, in which case, fNextToken
				// will be pointing to the right thing.  Otherwise,
				// we want to advance to the next token.
				/*
				int32 numCharsChecked = 0;
				PRBool allWhitespace = TRUE;
				while ((numCharsChecked < bytesToCopy)&& allWhitespace)
				{
					allWhitespace = (XP_STRCHR(WHITESPACE, fCurrentLine[numCharsChecked]) != NULL);
					numCharsChecked++;
				}

				if (!allWhitespace)
				{
					//fNextToken = fCurrentLine + bytesToCopy;
					fNextToken = GetNextToken();
					if (!nsCRT::strcmp(fNextToken, CRLF))
						fAtEndOfLine = TRUE;
				}
				*/
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
	int numOpenParens = 1;
	// count the number of parens in the current token
	int count, tokenLen = PL_strlen(fNextToken);
	for (count = 1; (count < tokenLen) && (numOpenParens > 0); count++)
	{
		if (fNextToken[count] == '(')
			numOpenParens++;
		else if (fNextToken[count] == ')')
			numOpenParens--;
	}

	// build up a buffer with the paren group.
	nsString2 buf;
	if ((numOpenParens > 0) && ContinueParse())
	{
		// First, copy that first token from before
		buf += fNextToken;

		buf += " ";	// space that got stripped off the token

		// Go through the current line and look for the last close paren.
		// We're not trying to parse it just yet, just separate it out.
		int len = PL_strlen(fCurrentTokenPlaceHolder);
		for (count = 0; (count < len) && (numOpenParens > 0); count++)
		{
			if (fCurrentTokenPlaceHolder[count] == '(')
				numOpenParens++;
			else if (fCurrentTokenPlaceHolder[count] == ')')
				numOpenParens--;
		}

		if (count < len)
		{
			// we found the last close paren.
			// Set fNextToken, fCurrentTokenPlaceHolder, etc.
			char oldChar = fCurrentTokenPlaceHolder[count];
			fCurrentTokenPlaceHolder[count] = 0;
			buf += fCurrentTokenPlaceHolder;
			fCurrentTokenPlaceHolder[count] = oldChar;
			fCurrentTokenPlaceHolder = fCurrentTokenPlaceHolder + count;
			fNextToken = GetNextToken();
		}
		else
		{
			// there should always be either a space or CRLF after the response, right?
			SetSyntaxError(TRUE);
		}
	}
	else if ((numOpenParens == 0) && ContinueParse())
	{
		// the whole paren group response was a single token
		buf = fNextToken;
	}

	if (numOpenParens < 0)
		SetSyntaxError(TRUE);

	return buf.ToNewCString();
}

