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

#include "numerics.h"
#include "parser.h"
#include "world.h"

namespace JS = JavaScript;


//
// Reader
//


// Create a Reader reading characters from begin up to but not including end.
JS::Reader::Reader(const char16 *begin, const char16 *end):
	begin(begin), p(begin), end(end), lineStart(begin), nGetsPastEnd(0)
{
	ASSERT(begin <= end);
  #ifdef DEBUG
	recordString = 0;
  #endif
}


// Unread the last n characters.  unget cannot be called to back up past the position
// of the last call to beginLine().
void JS::Reader::unget(uint32 n)
{
	if (nGetsPastEnd) {
		if (nGetsPastEnd >= n) {
			nGetsPastEnd -= n;
			return;
		}
		n -= nGetsPastEnd;
		nGetsPastEnd = 0;
	}
	ASSERT(p >= begin + n);
	p -= n;
}


// Return the characters read in from position begin inclusive to position end
// exclusive relative to the current line.  begin <= end <= charPos() is required.
JS::String JS::Reader::extract(uint32 begin, uint32 end) const
{
	ASSERT(begin <= end && end + nGetsPastEnd <= charPos());
	return String(lineStart + begin, lineStart + end);
}


// Begin accumulating characters into the recordString, whose initial value is
// ignored and cleared.  Each character passed to recordChar() is added to the end
// of the recordString.  Recording ends when endRecord() or beginLine() is called.
// Recording is significantly optimized when the characters passed to readChar()
// are the same characters as read by get().  In this case the record String does
// not get allocated until endRecord() is called or a discrepancy appears between
// get() and recordChar().
void JS::Reader::beginRecording(String &recordString)
{
	Reader::recordString = &recordString;
	recordBase = p;
	recordPos = p;
}


// Append ch to the recordString.
void JS::Reader::recordChar(char16 ch)
{
	ASSERT(recordString);
	if (recordPos) {
		if (recordPos != end && *recordPos == ch) {
			recordPos++;
			return;
		} else {
			recordString->assign(recordBase, recordPos);
			recordPos = 0;
		}
	}
	*recordString += ch;
}


// Finish recording characters into the recordString that was last passed to beginRecording().
// Return that recordString.
JS::String &JS::Reader::endRecording()
{
	String *rs = recordString;
	ASSERT(rs);
	if (recordPos)
		rs->assign(recordBase, recordPos);
	recordString = 0;
	return *rs;
}


// Refill the source buffer after running off the end.  Get and return
// the next character.
// The default implementation just returns char16eof.
JS::char16orEOF JS::Reader::underflow()
{
	++nGetsPastEnd;
	return char16eof;
}


// Perform a peek when begin == end.
JS::char16orEOF JS::Reader::peekUnderflow()
{
	char16orEOF ch = underflow();
	unget();
	return ch;
}


// Create a StringReader reading characters from s.
// source describes the origin of string s and may be used for error messages.
JS::StringReader::StringReader(const String &s, const String &source):
	str(s), source(source)
{
	const char16 *begin = str.data();
	setBuffer(begin, begin, begin + str.size());
}


JS::String JS::StringReader::sourceFile() const
{
	return source;
}


//
// Lexer
//


const char *const JS::Token::kindNames[] = {
  // Special
	"[End]",			// Token::End
	"[Identifier]",		// Token::Id
	"[Numeral]",		// Token::Num
	"[String]",			// Token::Str
	"[Unit]",			// Token::Unit
	"[RegExp]",			// Token::RegExp

  // Punctuators
	"(",				// Token::OpenParenthesis
	")",				// Token::CloseParenthesis
	"[",				// Token::OpenBracket
	"]",				// Token::CloseBracket
	"{",				// Token::OpenBrace
	"}",				// Token::CloseBrace
	",",				// Token::Comma
	";",				// Token::Semicolon
	".",				// Token::Dot
	"..",				// Token::DoubleDot
	"...",				// Token::TripleDot
	"->",				// Token::Arrow
	":",				// Token::Colon
	"::",				// Token::DoubleColon
	"#",				// Token::Pound
	"@",				// Token::At
	"++",				// Token::Increment
	"--",				// Token::Decrement
	"~",				// Token::Complement
	"!",				// Token::Not
	"*",				// Token::Times
	"/",				// Token::Divide
	"%",				// Token::Modulo
	"+",				// Token::Plus
	"-",				// Token::Minus
	"<<",				// Token::LeftShift
	">>",				// Token::RightShift
	">>>",				// Token::LogicalRightShift
	"&&",				// Token::LogicalAnd
	"^^",				// Token::LogicalXor
	"||",				// Token::LogicalOr
	"&",				// Token::And
	"^",				// Token::Xor
	"|",				// Token::Or
	"=",				// Token::Assignment
	"*=",				// Token::TimesEquals
	"/=",				// Token::DivideEquals
	"%=",				// Token::ModuloEquals
	"+=",				// Token::PlusEquals
	"-=",				// Token::MinusEquals
	"<<=",				// Token::LeftShiftEquals
	">>=",				// Token::RightShiftEquals
	">>>=",				// Token::LogicalRightShiftEquals
	"&&=",				// Token::LogicalAndEquals
	"^^=",				// Token::LogicalXorEquals
	"||=",				// Token::LogicalOrEquals
	"&=",				// Token::AndEquals
	"^=",				// Token::XorEquals
	"|=",				// Token::OrEquals
	"==",				// Token::Equal
	"!=",				// Token::NotEqual
	"<",				// Token::LessThan
	"<=",				// Token::LessThanOrEqual
	">",				// Token::GreaterThan
	">=",				// Token::GreaterThanOrEqual
	"===",				// Token::Identical
	"!==",				// Token::NotIdentical
	"?",				// Token::Question

  // Reserved words
	"abstract",			// Token::Abstract
	"break",			// Token::Break
	"case",				// Token::Case
	"catch",			// Token::Catch
	"class",			// Token::Class
	"const",			// Token::Const
	"continue",			// Token::Continue
	"debugger",			// Token::Debugger
	"default",			// Token::Default
	"delete",			// Token::Delete
	"do",				// Token::Do
	"else",				// Token::Else
	"enum",				// Token::Enum
	"eval",				// Token::Eval
	"export",			// Token::Export
	"extends",			// Token::Extends
	"false",			// Token::False
	"final",			// Token::Final
	"finally",			// Token::Finally
	"for",				// Token::For
	"function",			// Token::Function
	"goto",				// Token::Goto
	"if",				// Token::If
	"implements",		// Token::Implements
	"import",			// Token::Import
	"in",				// Token::In
	"instanceof",		// Token::Instanceof
	"native",			// Token::Native
	"new",				// Token::New
	"null",				// Token::Null
	"package",			// Token::Package
	"private",			// Token::Private
	"protected",		// Token::Protected
	"public",			// Token::Public
	"return",			// Token::Return
	"static",			// Token::Static
	"super",			// Token::Super
	"switch",			// Token::Switch
	"synchronized",		// Token::Synchronized
	"this",				// Token::This
	"throw",			// Token::Throw
	"throws",			// Token::Throws
	"transient",		// Token::Transient
	"true",				// Token::True
	"try",				// Token::Try
	"typeof",			// Token::Typeof
	"var",				// Token::Var
	"volatile",			// Token::Volatile
	"while",			// Token::While
	"with",				// Token::With

  // Non-reserved words
	"box",				// Token::Box
	"constructor",		// Token::Constructor
	"field",			// Token::Field
	"get",				// Token::Get
	"language",			// Token::Language
	"local",			// Token::Local
	"method",			// Token::Method
	"override",			// Token::Override
	"set",				// Token::Set
	"version"			// Token::Version
};


// Initialize the keywords in the given world.
void JS::Token::initKeywords(World &world)
{
	const char *const*keywordName = kindNames + KeywordsBegin;
	for (Kind kind = KeywordsBegin; kind != KeywordsEnd; kind = Kind(kind+1))
		world.identifiers[widenCString(*keywordName++)].tokenKind = kind;
}


// Append a description of the token to dst.
void JS::Token::print(String &dst, bool debug) const
{
	switch (kind) {
	  case Id:
		if (debug)
			dst += "[Id]";
		dst += *identifier;
		break;

	  case Num:
		if (debug) {
			dst += "[Num ";
			dst += value;
			dst += ']';
		}
		dst += chars;
		break;

	  case Unit:
		if (debug)
			dst += "[Unit]";
	  case Str:
		dst += '"';
		dst += chars;
		dst += '"';
		break;

	  case RegExp:
		dst += '/';
		dst += *identifier;
		dst += '/';
		dst += chars;
		break;

	  default:
		dst += kind;
	}
}


// Create a new Lexer using the provided Reader and interning identifiers, keywords, and regular
// expressions in the designated world.
JS::Lexer::Lexer(Reader &reader, World &world): reader(reader), world(world)
{
	nextToken = tokens;
	nTokensFwd = 0;
  #ifdef DEBUG
	nTokensBack = 0;
  #endif
	lineNum = 1;
	lexingUnit = false;
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


// Report a syntax error at the backUp-th last character read by the Reader.
// In other words, if backUp is 0, the error is at the next character to be read by the Reader;
// if backUp is 1, the error is at the last character read by the Reader, and so forth.
void JS::Lexer::syntaxError(const char *message, uint backUp)
{
	reader.unget(backUp);
	uint32 charPos = reader.charPos();
	char16orEOF ch;
	do {
		ch = reader.get();
	} while (ch != char16eof && !isLineBreak(char16orEOFToChar16(ch)));
	reader.unget();
	Exception e(Exception::SyntaxError, widenCString(message), reader.sourceFile(), lineNum, charPos,
				reader.extract(0, reader.charPos()));
	throw e;
}


// Get the next character from the reader, skipping any Unicode format-control (Cf) characters.
inline JS::char16orEOF JS::Lexer::getChar()
{
	char16orEOF ch = reader.get();
	if (static_cast<uint32>(ch) >= firstFormatChar)
		ch = internalGetChar(ch);
	return ch;
}

// Helper for getChar()
JS::char16orEOF JS::Lexer::internalGetChar(char16orEOF ch)
{
	while (isFormat(char16orEOFToChar16(ch)))
		ch = reader.get();
	return ch;
}


// Peek the next character from the reader, skipping any Unicode format-control (Cf) characters,
// which are read and discarded.
inline JS::char16orEOF JS::Lexer::peekChar()
{
	char16orEOF ch = reader.peek();
	if (static_cast<uint32>(ch) >= firstFormatChar)
		ch = internalPeekChar(ch);
	return ch;
}

// Helper for peekChar()
JS::char16orEOF JS::Lexer::internalPeekChar(char16orEOF ch)
{
	while (isFormat(char16orEOFToChar16(ch))) {
		reader.get();
		ch = reader.peek();
	}
	return ch;
}


// Peek the next character from the reader, skipping any Unicode format-control (Cf) characters,
// which are read and discarded.  If the peeked character matches ch, read that character and return true;
// otherwise return false.
bool JS::Lexer::testChar(char16 ch)
{
	char16orEOF ch2 = peekChar();
	if (ch == ch2) {
		reader.get();
		return true;
	}
	return false;
}


// A backslash has been read.  Read the rest of the escape code.
// Return the interpreted escaped character.  Throw an exception if the escape is not valid.
// If unicodeOnly is true, allow only \uxxxx escapes.
char16 JS::Lexer::lexEscape(bool unicodeOnly)
{
	char16orEOF ch = getChar();
	int nDigits;

	if (!unicodeOnly || ch == 'u')
		switch (ch) {
		  case '0':
			// Make sure that the next character isn't a digit.
			ch = peekChar();
			if (!isASCIIDecimalDigit(char16orEOFToChar16(ch)))
				return 0x00;
			getChar();	// Point to the next character in the error message
		  case 'b':
			return 0x08;
		  case 'f':
			return 0x0C;
		  case 'n':
			return 0x0A;
		  case 'r':
			return 0x0D;
		  case 't':
			return 0x09;
		  case 'v':
			return 0x0B;
		  case 'x':
			nDigits = 2;
			goto lexHex;
		  case 'u':
			nDigits = 4;
		  lexHex:
			{
				uint32 n = 0;
				while (nDigits--) {
					ch = getChar();
					uint digit;
					if (!isASCIIHexDigit(char16orEOFToChar16(ch), digit))
						goto error;
					n = (n << 4) | digit;
				}
				return char16(n);
			}
		default:
			if (ch != char16eof) {
				CharInfo chi(char16orEOFToChar16(ch));
				if (!isAlphanumeric(chi) && !isLineBreak(chi))
					return char16orEOFToChar16(ch);
			}
		}
  error:
	syntaxError("Bad escape code");
	return 0;
}


// Read an identifier into s.  The initial value of s is ignored and cleared.
// Return true if an escape code has been encountered.
// If allowLeadingDigit is true, allow the first character of s to be a digit, just like any
// continuing identifier character.
bool JS::Lexer::lexIdentifier(String &s, bool allowLeadingDigit)
{
	reader.beginRecording(s);
	bool hasEscape = false;

	while (true) {
		char16orEOF ch = getChar();
		char16orEOF ch2 = ch;
		if (ch == '\\') {
			ch2 = lexEscape(true);
			hasEscape = true;
		}
		CharInfo chi2(char16orEOFToChar16(ch2));
		
		if (!(allowLeadingDigit ? isIdContinuing(chi2) : isIdLeading(chi2))) {
			if (ch == '\\')
				syntaxError("Identifier escape expands into non-identifier character");
			else
				reader.unget();
			break;
		}
		reader.recordChar(char16orEOFToChar16(ch2));
		allowLeadingDigit = true;
	}
	reader.endRecording();
	return hasEscape;
}


// Read a numeric literal into nextToken->chars and nextToken->value.
// Return true if the numeric literal is followed by a unit, but don't read the unit yet.
bool JS::Lexer::lexNumeral()
{
	int hasDecimalPoint = 0;
	String &s = nextToken->chars;
	uint digit;

	reader.beginRecording(s);
	char16orEOF ch = getChar();
	if (ch == '0') {
		reader.recordChar('0');
		ch = getChar();
		if ((ch&~0x20) == 'X') {
			uint32 pos = reader.charPos();
			char16orEOF ch2 = getChar();
			if (isASCIIHexDigit(char16orEOFToChar16(ch2), digit)) {
				reader.recordChar(char16orEOFToChar16(ch));
				do {
					reader.recordChar(char16orEOFToChar16(ch2));
					ch2 = getChar();
				} while (isASCIIHexDigit(char16orEOFToChar16(ch2), digit));
				ch = ch2;
			} else
				reader.backUpTo(pos);
			goto done;
		} else if (isASCIIDecimalDigit(char16orEOFToChar16(ch))) {
			syntaxError("Numeric constant syntax error");
		}
	}
	while (isASCIIDecimalDigit(char16orEOFToChar16(ch)) || ch == '.' && !hasDecimalPoint++) {
		reader.recordChar(char16orEOFToChar16(ch));
		ch = getChar();
	}
	if ((ch&~0x20) == 'E') {
		uint32 pos = reader.charPos();
		char16orEOF ch2 = getChar();
		char16 sign = 0;
		if (ch2 == '+' || ch2 == '-') {
			sign = char16orEOFToChar16(ch2);
			ch2 = getChar();
		}
		if (isASCIIDecimalDigit(char16orEOFToChar16(ch2))) {
			reader.recordChar(char16orEOFToChar16(ch));
			if (sign)
				reader.recordChar(sign);
			do {
				reader.recordChar(char16orEOFToChar16(ch2));
				ch2 = getChar();
			} while (isASCIIDecimalDigit(char16orEOFToChar16(ch2)));
			ch = ch2;
		} else
			reader.backUpTo(pos);
	}
	
  done:
	// At this point the reader is just past the character ch, which is the first non-formatting character
	// that is not part of the number.
	reader.endRecording();
	const char16 *sBegin = s.data();
	const char16 *sEnd = sBegin + s.size();
	const char16 *numEnd;
	nextToken->value = stringToDouble(sBegin, sEnd, numEnd);
	ASSERT(numEnd == sEnd);
	reader.unget();
	ASSERT(ch == reader.peek());
	return isIdContinuing(char16orEOFToChar16(ch)) || ch == '\\';
}


// Read a string literal into s.  The initial value of s is ignored and cleared.
// The opening quote has already been read into separator.
void JS::Lexer::lexString(String &s, char16 separator)
{
	char16orEOF ch;

	reader.beginRecording(s);
	while ((ch = reader.get()) != separator) {
    	CharInfo chi(char16orEOFToChar16(ch));
    	if (!isFormat(chi)) {
			if (ch == '\\')
				ch = lexEscape(false);
			else if (ch == char16eof || isLineBreak(chi))
				syntaxError("Unterminated string literal");
			reader.recordChar(char16orEOFToChar16(ch));
		}
	}
	reader.endRecording();
}


// Read a regular expression literal.  Store the regular expression in nextToken->identifier
// and the flags in nextToken->chars.
// The opening slash has already been read.
void JS::Lexer::lexRegExp()
{
	String s;
	char16orEOF prevCh = 0;

	reader.beginRecording(s);
	while (true) {
		char16orEOF ch = getChar();
    	CharInfo chi(char16orEOFToChar16(ch));
		if (ch == char16eof || isLineBreak(chi))
			syntaxError("Unterminated regular expression literal");
		if (prevCh == '\\') {
			reader.recordChar(char16orEOFToChar16(ch));
			prevCh = 0;	// Ignore slashes and backslashes immediately after a backslash
		} else if (ch != '/') {
			reader.recordChar(char16orEOFToChar16(ch));
			prevCh = ch;
		} else
			break;
	}
	reader.endRecording();
	nextToken->identifier = &world.identifiers[s];
	
	lexIdentifier(nextToken->chars, true);
}


// Read a token from the Reader and store it at *nextToken.
// If the Reader reached the end of file, store a Token whose Kind is End.
void JS::Lexer::lexToken(bool preferRegExp)
{
	Token &t = *nextToken;
	t.lineBreak = false;
	t.identifier = 0;
	//clear(t.chars);	// Don't really need to waste time clearing this string here
	t.value = 0.0;
	Token::Kind kind;

	if (lexingUnit) {
		lexIdentifier(t.chars, false);
		ASSERT(t.chars.size());
		kind = Token::Unit;				// unit
		lexingUnit = false;
	} else {
	  next:
		char16orEOF ch = reader.get();
		if (ch == char16eof) {
		  endOfInput:
			kind = Token::End;
		} else {
			char16orEOF ch2;
			CharInfo chi(char16orEOFToChar16(ch));

			switch (cGroup(chi)) {
		      case CharInfo::FormatGroup:
		      case CharInfo::WhiteGroup:
		    	goto next;

		      case CharInfo::IdGroup:
		    	t.charPos = reader.charPos() - 1;
		      readIdentifier:
		    	{
			    	reader.unget();
			    	String s;
		    		bool hasEscape = lexIdentifier(s, false);
			    	t.identifier = &world.identifiers[s];
			    	kind = hasEscape ? Token::Id : t.identifier->tokenKind;
		    	}
		    	break;

		      case CharInfo::NonIdGroup:
		      case CharInfo::IdContinueGroup:
		    	t.charPos = reader.charPos() - 1;
		    	switch (ch) {
				  case '(':
					kind = Token::OpenParenthesis;	// (
					break;
				  case ')':
					kind = Token::CloseParenthesis;	// )
					break;
				  case '[':
					kind = Token::OpenBracket;		// [
					break;
				  case ']':
					kind = Token::CloseBracket;		// ]
					break;
				  case '{':
					kind = Token::OpenBrace;		// {
					break;
				  case '}':
					kind = Token::CloseBrace;		// }
					break;
				  case ',':
					kind = Token::Comma;			// ,
					break;
				  case ';':
					kind = Token::Semicolon;		// ;
					break;
				  case '.':
					kind = Token::Dot;				// .
					ch2 = getChar();
					if (isASCIIDecimalDigit(char16orEOFToChar16(ch2))) {
						reader.backUpTo(t.charPos);
						goto number;				// decimal point
					} else if (ch2 == '.') {
						kind = Token::DoubleDot;	// ..
						if (testChar('.'))
							kind = Token::TripleDot; // ...
					} else
						reader.unget();
					break;
				  case ':':
					kind = Token::Colon;			// :
					if (testChar(':'))
						kind = Token::DoubleColon;	// ::
					break;
				  case '#':
					kind = Token::Pound;			// #
					break;
				  case '@':
					kind = Token::At;				// @
					break;
				  case '?':
					kind = Token::Question;			// ?
					break;

				  case '~':
					kind = Token::Complement;		// ~
					break;
				  case '!':
					kind = Token::Not;				// !
					if (testChar('=')) {
						kind = Token::NotEqual;		// !=
						if (testChar('='))
							kind = Token::NotIdentical; // !==
					}
					break;

				  case '*':
					kind = Token::Times;			// * *=
				  tryAssignment:
					if (testChar('='))
						kind = Token::Kind(kind + Token::TimesEquals - Token::Times);
					break;

				  case '/':
					kind = Token::Divide;			// /
					ch = getChar();
					if (ch == '/') {				// // comment
						do {
							ch = reader.get();
							if (ch == char16eof)
								goto endOfInput;
						} while (!isLineBreak(char16orEOFToChar16(ch)));
						goto endOfLine;
					} else if (ch == '*') {			// /* comment */
						ch = 0;
						do {
							ch2 = ch;
							ch = getChar();
							if (isLineBreak(char16orEOFToChar16(ch))) {
								reader.beginLine();
								++lineNum;
								t.lineBreak = true;
							}
							if (ch == char16eof)
								syntaxError("Unterminated /* comment");
						} while (ch != '/' || ch2 != '*');
						goto next;
					} else {
						reader.unget();
						if (preferRegExp) {			// Regular expression
							kind = Token::RegExp;
							lexRegExp();
						} else
							 goto tryAssignment;	// /=
					}
					break;

				  case '%':
					kind = Token::Modulo;			// %
					goto tryAssignment;				// %=

				  case '+':
					kind = Token::Plus;				// +
					if (testChar('+'))
						kind = Token::Increment;	// ++
					else
						goto tryAssignment;			// +=
					break;

				  case '-':
					kind = Token::Minus;			// -
					ch = getChar();
					if (ch == '-')
						kind = Token::Decrement;	// --
					else if (ch == '>')
						kind = Token::Arrow;		// ->
					else {
						reader.unget();
						goto tryAssignment;			// -=
					}
					break;
			
				  case '&':
					kind = Token::And;				// & && &= &&=
				  logical:
					if (testChar(char16orEOFToChar16(ch)))
						kind = Token::Kind(kind - Token::And + Token::LogicalAnd);
					goto tryAssignment;
				  case '^':
					kind = Token::Xor;				// ^ ^^ ^= ^^=
					goto logical;
				  case '|':
					kind = Token::Or;				// | || |= ||=
					goto logical;

				  case '=':
					kind = Token::Assignment;		// =
					if (testChar('=')) {
						kind = Token::Equal;		// ==
						if (testChar('='))
							kind = Token::Identical; // ===
					}
					break;

				  case '<':
					kind = Token::LessThan;			// <
					if (testChar('<')) {
						kind = Token::LeftShift;	// <<
						goto tryAssignment;			// <<=
					}
				  comparison:
					if (testChar('='))				// <= >=
						kind = Token::Kind(kind + Token::LessThanOrEqual - Token::LessThan);
					break;
				  case '>':
					kind = Token::GreaterThan;		// >
					if (testChar('>')) {
						kind = Token::RightShift;	// >>
						if (testChar('>'))
							kind = Token::LogicalRightShift; // >>>
						goto tryAssignment;			// >>= >>>=
					}
					goto comparison;

				  case '\\':
					goto readIdentifier;			// An identifier that starts with an escape

				  case '\'':
				  case '"':
					kind = Token::Str;				// 'string' "string"
					lexString(t.chars, char16orEOFToChar16(ch));
					break;

				  case '0':
				  case '1':
				  case '2':
				  case '3':
				  case '4':
				  case '5':
				  case '6':
				  case '7':
				  case '8':
				  case '9':
					reader.unget();					// Number
				  number:
					kind = Token::Num;
					lexingUnit = lexNumeral();
					break;

				  default:
					syntaxError("Bad character");
		    	}
		    	break;

		      case CharInfo::LineBreakGroup:
		      endOfLine:
				reader.beginLine();
				++lineNum;
				t.lineBreak = true;
				goto next;
			}
		}
	}
	t.kind = kind;
	t.lineNum = lineNum;
}
