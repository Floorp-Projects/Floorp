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
#include "world.h"

namespace JavaScript {

//
// Reader
//

	// A Reader reads Unicode characters from some source -- either a file or a string.
	// get() returns all of the characters followed by a ueof.
	class Reader {
		const char16 *begin;			// Beginning of current buffer
		const char16 *p;				// Position in current buffer
		const char16 *end;				// End of current buffer
		const char16 *markPos;			// Pointer to mark in current buffer or null if no mark
		uint32 nGetsPastEnd;			// Number of times ueof has been returned

	  protected:
		Reader(): nGetsPastEnd(0) {}
	  public:
		Reader(const char16 *begin, const char16 *end);
	  private:
	    Reader(const Reader&);			// No copy constructor
	    void operator=(const Reader&);	// No assignment operator
	  public:
	#ifdef DEBUG
		~Reader() {ASSERT(!markPos);}
	#endif

		wint_t get();
		wint_t peek();
		void unget();
		
		void mark();
		void unmark();
		void unmark(String &s);
		bool marked() const {return markPos;}
	
	  protected:
		void setBuffer(const char16 *begin, const char16 *p, const char16 *end);
		virtual wint_t underflow();
		wint_t peekUnderflow();
	};


	// Get and return the next character or ueof if at end of input.
	inline wint_t Reader::get()
	{
		if (p != end)
			return *p++;
		return underflow();
	}

	// Return the next character without consuming it.  Return ueof if at end of input.
	inline wint_t Reader::peek()
	{
		if (p != end)
			return *p;
		return peekUnderflow();
	}

	// Mark the current position in the Reader.
	inline void Reader::mark()
	{
		ASSERT(!markPos);
		markPos = p;
	}

	// Delete the Reader mark.
	inline void Reader::unmark()
	{
		ASSERT(markPos);
		markPos = 0;
	}


	inline void Reader::setBuffer(const char16 *begin, const char16 *p, const char16 *end)
	{
		ASSERT(begin <= p && p <= end);
		Reader::begin = begin;
		Reader::p = p;
		Reader::end = end;
	}


	// A Reader that reads from a String.
	class StringReader: public Reader {
		const String str;
	  public:
		StringReader(const String &s);
	};


//
// Lexer
//

	class Token {
	  public:
		enum Kind {
			End,						// End of token stream
			Error,						// Lexer error

			Id,							// Non-keyword identifier (may be same as a keyword if it contains an escape code)
			Num,						// Numeral
			Str,						// String or unit after numeral
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
			And,						// &
			Xor,						// ^
			Or,							// |

			Assignment,					// =
			TimesEquals,				// *=
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
			GreaterThan,				// >
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

		Kind kind;						// The token's kind
		bool lineBreak;					// True if line break precedes this token
		uint32 lineNum;					// One-based source line number
		uint32 charPos;					// Zero-based character offset of this token in source line
		StringAtom *identifier;			// The token's characters (identifiers, keywords, and regular expressions only)
		auto_ptr<String> chars;			// The token's characters (strings, numbers, and regular expression flags only)
		float64 value;					// The token's value (numbers only)
	};


	class Lexer {
		static const int tokenBufferSize = 3;	// Token lookahead buffer size
	  public:
		Reader &reader;
	  private:
		Token tokens[tokenBufferSize];	// Circular buffer of recently read or lookahead tokens
		Token *nextToken;				// Address of next Token in the circular buffer to be returned by get()
		int nTokensFwd;					// Net number of Tokens on which unget() has been called; these Tokens are ahead of nextToken
	  #ifdef DEBUG
		int nTokensBack;				// Number of Tokens on which unget() can be called; these Tokens are beind nextToken
		bool savedPreferRegExp[tokenBufferSize]; // Circular buffer of saved values of preferRegExp to get() calls
	  #endif

	  public:
		Lexer(Reader &reader);
		
		Token &get(bool preferRegExp);
		const Token &peek(bool preferRegExp);
		void unget();

	  private:
		void lexToken(bool preferRegExp);
	};
}
#endif
