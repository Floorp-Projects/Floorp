// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// The contents of this file are subject to the Netscape Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/NPL/
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
// implied. See the License for the specific language governing
// rights and limitations under the License.
//
// The Original Code is the JavaScript 2 Prototype.
//
// The Initial Developer of the Original Code is Netscape
// Communications Corporation.  Portions created by Netscape are
// Copyright (C) 1998 Netscape Communications Corporation. All
// Rights Reserved.

#include "parser.h"
namespace JS = JavaScript;


//
// Reader
//


// Create a Reader reading characters from begin up to but not including end.
JS::Reader::Reader(const char16 *begin, const char16 *end):
	begin(begin), p(begin), end(end), nGetsPastEnd(0)
{
	ASSERT(begin <= end);
}


// Unread the last character.
void JS::Reader::unget()
{
	if (nGetsPastEnd)
		--nGetsPastEnd;
	else {
		ASSERT(p != begin);
		--p;
	}
}


// Set s to the characters read in after the mark but before the current position
// and then delete the Reader mark.
void JS::Reader::unmark(String &s)
{
	ASSERT(markPos);
	s.assign(markPos, p);
	markPos = 0;
}


// Refill the source buffer after running off the end.  Get and return
// the next character.
// The default implementation just returns ueof.
JS::wint_t JS::Reader::underflow()
{
	++nGetsPastEnd;
	return ueof;
}


// Perform a peek when begin == end.
JS::wint_t JS::Reader::peekUnderflow()
{
	wint_t ch = underflow();
	unget();
	return ch;
}


// Create a StringReader reading characters from a copy of the given String.
JS::StringReader::StringReader(const String &s):
	str(s)
{
	const char16 *begin = str.data();
	setBuffer(begin, begin, begin + str.size());
}


//
// Lexer
//


// Create a new Lexer using the provided Reader.
JS::Lexer::Lexer(Reader &reader): reader(reader)
{
	nextToken = tokens;
	nTokensFwd = 0;
  #ifdef DEBUG
	nTokensBack = 0;
  #endif
}


// Get and return the next token.  The token remains valid until the next call to this Lexer.
// If the Reader reached the end of file, return a Token whose Kind is End.
// The caller may alter the value of this Token (in particular, take control over the
// auto_ptr's data), but if it does so, the caller is not allowed to unget this Token.
//
// If preferRegExp is true, a / will be preferentially interpreted as starting a regular
// expression; otherwise, a / will be preferentially interpreted as division or /=.
JS::Token &JS::Lexer::get(bool preferRegExp)
{
	Token &t = const_cast<Token &>(peek(preferRegExp));
	if (++nextToken == tokens + tokenBufferSize)
		nextToken = tokens;
	--nTokensFwd;
	DEBUG_ONLY(++nTokensBack);
	return t;
}


// Return the next token without consuming it.
//
// If preferRegExp is true, a / will be preferentially interpreted as starting a regular
// expression; otherwise, a / will be preferentially interpreted as division or /=.
// A subsequent call to peek or get will return the same token; that call must be presented
// with the same value for preferRegExp.
const JS::Token &JS::Lexer::peek(bool preferRegExp)
{
	// Use an already looked-up token if there is one.
	if (nTokensFwd) {
		ASSERT(savedPreferRegExp[nextToken - tokens] == preferRegExp);
	} else {
		lexToken(preferRegExp);
		nTokensFwd = 1;
	  #ifdef DEBUG
		savedPreferRegExp[nextToken - tokens] = preferRegExp;
		if (nTokensBack == tokenBufferSize)
			nTokensBack = tokenBufferSize-1;
	  #endif
	}
	return *nextToken;
}


// Unread the last token.  This call may be called to unread at most tokenBufferSize tokens
// at a time (where a peek also counts as temporarily reading and unreading one token).
// When a token that has been unread is peeked or read again, the same value must be passed
// in preferRegExp as for the first time that token was read or peeked.
void JS::Lexer::unget()
{
	ASSERT(nTokensBack--);
	nTokensFwd++;
	if (nextToken == tokens)
		nextToken = tokens + tokenBufferSize;
	--nextToken;
}


// Read a token from the Reader and store it at *nextToken.
// If the Reader reached the end of file, store a Token whose Kind is End.
void JS::Lexer::lexToken(bool preferRegExp)
{
}

