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

#ifndef parser_h
#define parser_h

#include "utilities.h"

namespace JavaScript {

	class StringAtom;
	class World;

//
// Reader
//

	// A Reader reads Unicode characters from some source -- either a file or a string.
	// get() returns all of the characters followed by a char16eof.
	// If get() returns LF (u000A), CR (u000D), LS (u2028), or PS (u2029), then beginLine()
	// must be called before getting or peeking any more characters.
	class Reader {
	  protected:
		const char16 *begin;			// Beginning of current buffer
		const char16 *p;				// Position in current buffer
		const char16 *end;				// End of current buffer
		const char16 *lineStart;		// Pointer to start of current line
		uint32 nGetsPastEnd;			// Number of times char16eof has been returned
	  public:
		uint32 lineNum;					// One-based number of current line
		FileOffset lineFileOffset;		// Byte or character offset of start of current line relative to all of input
	  private:

		String *recordString;			// String, if any, into which recordChar() records characters
		const char16 *recordBase;		// Position of last beginRecording() call
		const char16 *recordPos;		// Position of last recordChar() call; nil if a discrepancy occurred
		
	  protected:
		Reader(): nGetsPastEnd(0), lineNum(1), lineFileOffset(0) {}
	  public:
		Reader(const char16 *begin, const char16 *end);
	  private:
	    Reader(const Reader&);			// No copy constructor
	    void operator=(const Reader&);	// No assignment operator
	  public:

		char16orEOF get();
		char16orEOF peek();
		void unget(uint32 n = 1);
		
		virtual void beginLine() = 0;
		uint32 charPos() const;
		void backUpTo(uint32 pos);

		String extract(uint32 begin, uint32 end) const;
		void beginRecording(String &recordString);
		void recordChar(char16 ch);
		String &endRecording();
		
		virtual String sourceFile() const = 0; // A description of the source code that caused the error

	  protected:
		void setBuffer(const char16 *begin, const char16 *p, const char16 *end);
		virtual char16orEOF underflow();
		char16orEOF peekUnderflow();
	};


	// Get and return the next character or char16eof if at end of input.
	inline char16orEOF Reader::get()
	{
		if (p != end)
			return *p++;
		return underflow();
	}

	// Return the next character without consuming it.  Return char16eof if at end of input.
	inline char16orEOF Reader::peek()
	{
		if (p != end)
			return *p;
		return peekUnderflow();
	}


	// Return the number of characters between the current position and the beginning of the current line.
	// This cannot be called if the current position is past the end of the input.
	inline uint32 Reader::charPos() const
	{
		ASSERT(!nGetsPastEnd);
		return static_cast<uint32>(p - lineStart);
	}


	// Back up to the given character offset relative to the current line.
	inline void Reader::backUpTo(uint32 pos)
	{
		ASSERT(pos <= charPos());
		p = lineStart + pos;
		nGetsPastEnd = 0;
	}


	inline void Reader::setBuffer(const char16 *begin, const char16 *p, const char16 *end)
	{
		ASSERT(begin <= p && p <= end);
		Reader::begin = begin;
		Reader::p = p;
		Reader::end = end;
		lineStart = begin;
	  #ifdef DEBUG
		recordString = 0;
	  #endif
	}


	// A Reader that reads from a String.
	class StringReader: public Reader {
		const String str;
		const String source;

	  public:
		StringReader(const String &s, const String &source);
		void beginLine();
		String sourceFile() const;
	};


//
// Lexer
//

	class Token {
		static const char *const kindNames[];
	  public:
		enum Kind {	// Keep synchronized with kindNames table
		  // Special
			End,						// End of token stream

			Id,							// Non-keyword identifier (may be same as a keyword if it contains an escape code)
			Num,						// Numeral
			Str,						// String
			Unit,						// Unit after numeral
			RegExp,						// Regular expression

		  // Punctuators
			OpenParenthesis,			// (
			CloseParenthesis,			// )
			OpenBracket,				// [
			CloseBracket,				// ]
			OpenBrace,					// {
			CloseBrace,					// }

			Comma,						// ,
			Semicolon,					// ;
			Dot,						// .
			DoubleDot,					// ..
			TripleDot,					// ...
			Arrow,						// ->
			Colon,						// :
			DoubleColon,				// ::
			Pound,						// #
			At,							// @

			Increment,					// ++
			Decrement,					// --

			Complement,					// ~
			Not,						// !

			Times,						// *
			Divide,						// /
			Modulo,						// %
			Plus,						// +
			Minus,						// -
			LeftShift,					// <<
			RightShift,					// >>
			LogicalRightShift,			// >>>
			LogicalAnd,					// &&
			LogicalXor,					// ^^
			LogicalOr,					// ||
			And,						// &	// These must be at constant offsets from LogicalAnd ... LogicalOr
			Xor,						// ^
			Or,							// |

			Assignment,					// =
			TimesEquals,				// *=	// These must be at constant offsets from Times ... Or
			DivideEquals,				// /=
			ModuloEquals,				// %=
			PlusEquals,					// +=
			MinusEquals,				// -=
			LeftShiftEquals,			// <<=
			RightShiftEquals,			// >>=
			LogicalRightShiftEquals,	// >>>=
			LogicalAndEquals,			// &&=
			LogicalXorEquals,			// ^^=
			LogicalOrEquals,			// ||=
			AndEquals,					// &=
			XorEquals,					// ^=
			OrEquals,					// |=

			Equal,						// ==
			NotEqual,					// !=
			LessThan,					// <
			LessThanOrEqual,			// <=
			GreaterThan,				// >	// >, >= must be at constant offsets from <, <=
			GreaterThanOrEqual,			// >=
			Identical,					// ===
			NotIdentical,				// !==

			Question,					// ?

		  // Reserved words
			Abstract,					// abstract
			Break,						// break
			Case,						// case
			Catch,						// catch
			Class,						// class
			Const,						// const
			Continue,					// continue
			Debugger,					// debugger
			Default,					// default
			Delete,						// delete
			Do,							// do
			Else,						// else
			Enum,						// enum
			Eval,						// eval
			Export,						// export
			Extends,					// extends
			False,						// false
			Final,						// final
			Finally,					// finally
			For,						// for
			Function,					// function
			Goto,						// goto
			If,							// if
			Implements,					// implements
			Import,						// import
			In,							// in
			Instanceof,					// instanceof
			Native,						// native
			New,						// new
			Null,						// null
			Package,					// package
			Private,					// private
			Protected,					// protected
			Public,						// public
			Return,						// return
			Static,						// static
			Super,						// super
			Switch,						// switch
			Synchronized,				// synchronized
			This,						// this
			Throw,						// throw
			Throws,						// throws
			Transient,					// transient
			True,						// true
			Try,						// try
			Typeof,						// typeof
			Var,						// var
			Volatile,					// volatile
			While,						// while
			With,						// with

		  // Non-reserved words
			Box,						// box
			Constructor,				// constructor
			Field,						// field
			Get,						// get
			Language,					// language
			Local,						// local
			Method,						// method
			Override,					// override
			Set,						// set
			Version,					// version
			
			KeywordsEnd,				// End of range of special identifier tokens
			KeywordsBegin = Abstract,	// Beginning of range of special identifier tokens
			KindsEnd = KeywordsEnd		// End of token kinds
		};

		Kind kind;						// The token's kind
		bool lineBreak;					// True if line break precedes this token
		SourcePosition pos;				// Position of this token
		StringAtom *identifier;			// The token's characters; non-null for identifiers, keywords, and regular expressions only
		String chars;					// The token's characters; valid for strings, units, numbers, and regular expression flags only
		float64 value;					// The token's value (numbers only)
		
		static void initKeywords(World &world);

		friend String &operator+=(String &s, Kind k) {ASSERT(uint(k) < KindsEnd); return s += kindNames[k];}
		friend String &operator+=(String &s, const Token &t) {t.print(s); return s;}
		void print(String &dst, bool debug = false) const;
	};


	class Lexer {
		enum {tokenBufferSize = 3};		// Token lookahead buffer size
	  public:
		Reader &reader;
		World &world;
	  private:
		Token tokens[tokenBufferSize];	// Circular buffer of recently read or lookahead tokens
		Token *nextToken;				// Address of next Token in the circular buffer to be returned by get()
		int nTokensFwd;					// Net number of Tokens on which unget() has been called; these Tokens are ahead of nextToken
	  #ifdef DEBUG
		int nTokensBack;				// Number of Tokens on which unget() can be called; these Tokens are beind nextToken
		bool savedPreferRegExp[tokenBufferSize]; // Circular buffer of saved values of preferRegExp to get() calls
	  #endif
		bool lexingUnit;				// True if lexing a unit identifier immediately following a number

	  public:
		Lexer(Reader &reader, World &world);
		
		Token &get(bool preferRegExp);
		const Token &peek(bool preferRegExp);
		void unget();

	  private:
		void syntaxError(const char *message, uint backUp = 1);
		char16orEOF getChar();
		char16orEOF internalGetChar(char16orEOF ch);
		char16orEOF peekChar();
		char16orEOF internalPeekChar(char16orEOF ch);
		bool testChar(char16 ch);

		char16 lexEscape(bool unicodeOnly);
		bool lexIdentifier(String &s, bool allowLeadingDigit);
		bool lexNumeral();
		void lexString(String &s, char16 separator);
		void lexRegExp();
		void lexToken(bool preferRegExp);
	  public:
	};


	class ParseNode {
		enum Kind {
			Empty,						// Empty (used in array literals, argument lists, etc.)
			Id,							// Identifier
			Num,						// Numeral
			Str,						// String
			Unit,						// Unit after numeral
			RegExp,						// Regular expression

		  // Punctuators
			OpenParenthesis,			// (
			CloseParenthesis,			// )
			OpenBracket,				// [
			CloseBracket,				// ]
			OpenBrace,					// {
			CloseBrace,					// }

			Comma,						// ,
			Semicolon,					// ;
			Dot,						// .
			DoubleDot,					// ..
			TripleDot,					// ...
			Arrow,						// ->
			Colon,						// :
			DoubleColon,				// ::
			Pound,						// #
			At,							// @

			Increment,					// ++
			Decrement,					// --

			Complement,					// ~
			Not,						// !

			Times,						// *
			Divide,						// /
			Modulo,						// %
			Plus,						// +
			Minus,						// -
			LeftShift,					// <<
			RightShift,					// >>
			LogicalRightShift,			// >>>
			LogicalAnd,					// &&
			LogicalXor,					// ^^
			LogicalOr,					// ||
			And,						// &	// These must be at constant offsets from LogicalAnd ... LogicalOr
			Xor,						// ^
			Or,							// |

			Assignment,					// =
			TimesEquals,				// *=	// These must be at constant offsets from Times ... Or
			DivideEquals,				// /=
			ModuloEquals,				// %=
			PlusEquals,					// +=
			MinusEquals,				// -=
			LeftShiftEquals,			// <<=
			RightShiftEquals,			// >>=
			LogicalRightShiftEquals,	// >>>=
			LogicalAndEquals,			// &&=
			LogicalXorEquals,			// ^^=
			LogicalOrEquals,			// ||=
			AndEquals,					// &=
			XorEquals,					// ^=
			OrEquals,					// |=

			Equal,						// ==
			NotEqual,					// !=
			LessThan,					// <
			LessThanOrEqual,			// <=
			GreaterThan,				// >	// >, >= must be at constant offsets from <, <=
			GreaterThanOrEqual,			// >=
			Identical,					// ===
			NotIdentical,				// !==

			Question,					// ?

		  // Reserved words
			Abstract,					// abstract
			Break,						// break
			Case,						// case
			Catch,						// catch
			Class,						// class
			Const,						// const
			Continue,					// continue
			Debugger,					// debugger
			Default,					// default
			Delete,						// delete
			Do,							// do
			Else,						// else
			Enum,						// enum
			Eval,						// eval
			Export,						// export
			Extends,					// extends
			False,						// false
			Final,						// final
			Finally,					// finally
			For,						// for
			Function,					// function
			Goto,						// goto
			If,							// if
			Implements,					// implements
			Import,						// import
			In,							// in
			Instanceof,					// instanceof
			Native,						// native
			New,						// new
			Null,						// null
			Package,					// package
			Private,					// private
			Protected,					// protected
			Public,						// public
			Return,						// return
			Static,						// static
			Super,						// super
			Switch,						// switch
			Synchronized,				// synchronized
			This,						// this
			Throw,						// throw
			Throws,						// throws
			Transient,					// transient
			True,						// true
			Try,						// try
			Typeof,						// typeof
			Var,						// var
			Volatile,					// volatile
			While,						// while
			With,						// with

		  // Non-reserved words
			Box,						// box
			Constructor,				// constructor
			Field,						// field
			Get,						// get
			Language,					// language
			Local,						// local
			Method,						// method
			Override,					// override
			Set,						// set
			Version						// version
		};
	};

	//class Parser: public Lexer {
	//};
}
#endif
