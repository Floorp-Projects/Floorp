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
		const StringAtom *identifier;	// The token's characters; non-null for identifiers, keywords, and regular expressions only
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
	};


//
// Language Selectors
//

	class Language {
		enum {
			minorVersion = 0,			// Single BCD digit representing the minor JavaScript version
			minorVersionBits = 4,
			majorVersion = 4,			// Single BCD digit representing the major JavaScript version
			majorVersionBits = 4,
			strictSemantics = 8,		// True when strict semantics are in use
			strictSyntax = 9,			// True when strict syntax is in use
			unknown = 31				// True when language is unknown
		};
		uint32 flags;					// Bitmap of flags at locations as above
	};

//
// Parser
//


	// The structures below are generally allocated inside an arena.  The structures' destructors
	// may never be called, so these structures should not hold onto any data that needs to be
	// destroyed explicitly.  Strings are allocated via newArenaString.

	struct ParseNode: ArenaObject {
		SourcePosition pos;				// Position of this statement or expression
		
		explicit ParseNode(const SourcePosition &pos): pos(pos) {}
	};

	struct FunctionName {
		enum Prefix {
			Normal,						// No prefix
			Get,						// get
			Set,						// set
			New							// new
		};
		
		Prefix prefix;					// The name's prefix, if any
		const StringAtom *identifier;	// The name; nil if omitted
		
		FunctionName(): prefix(Normal), identifier(0) {}
	};
	
	struct ExprNode;
	struct StmtNode;
	
	struct VariableBinding: ParseNode {
		VariableBinding *next;			// Next binding in a linked list of variable or parameter bindings
		const StringAtom *identifier;	// The variable's name; nil if omitted, which currently can only happen for ... parameters
		ExprNode *type;					// Type expression or nil if not provided
		ExprNode *initializer;			// Initial value expression or nil if not provided
		
		VariableBinding(const SourcePosition &pos): ParseNode(pos), next(0), identifier(0), type(0), initializer(0) {}
	};

	struct FunctionDefinition: FunctionName {
		VariableBinding *parameters;	// Linked list of all parameters, including optional and rest parameters, if any
		VariableBinding *optParameters;	// Pointer to first non-required parameter inside parameters list; nil if none
		VariableBinding *restParameter;	// Pointer to rest parameter inside parameters list; nil if none
		ExprNode *resultType;			// Result type expression or nil if not provided
		StmtNode *body;					// Body; nil if none
	};


	struct ExprNode: ParseNode {
		enum Kind {						// Actual class			Operands
			Id,							// IdentifierExprNode	<identifier>
			Num,						// NumberExprNode		<value>
			Str,						// StringExprNode		<str>
			RegExp,						// RegExpExprNode		/<regExp>/<flags>
			Null,						// ExprNode				null
			True,						// ExprNode				true
			False,						// ExprNode				false
			This,						// ExprNode				this
			Super,						// ExprNode				super

			NumUnit,					// NumUnitExprNode		<num> "<str>"   or   <num><str>
			ExprUnit,					// ExprUnitExprNode		(<op>) "<str>"
			QualifiedIdentifier,		// OpIdentifierExprNode	<op> :: <identifier>
			
			ObjectLiteral,				// PairListExprNode		{<field>:<value>, <field>:<value>, ..., <field>:<value>}
			ArrayLiteral,				// PairListExprNode		[<value>, <value>, ..., <value>]
			FunctionLiteral,			// FunctionExprNode		function <function>

			Call,						// InvokeExprNode		<op>(<field>:<value>, <field>:<value>, ..., <field>:<value>)
			New,						// InvokeExprNode		new <op>(<field>:<value>, <field>:<value>, ..., <field>:<value>)
			Index,						// InvokeExprNode		<op>[<field>:<value>, <field>:<value>, ..., <field>:<value>]

			Dot,						// BinaryExprNode		<op1> . <op2>  // <op2> must be Identifier or QualifiedIdentifier
			DotParen,					// BinaryExprNode		<op1> .( <op2> )
			At,							// BinaryExprNode		<op1> @ <op2>

			Delete,						// UnaryExprNode		delete <op>
			Typeof,						// UnaryExprNode		typeof <op>
			Eval,						// UnaryExprNode		eval <op>
			PreIncrement,				// UnaryExprNode		++ <op>
			PreDecrement,				// UnaryExprNode		-- <op>
			PostIncrement,				// UnaryExprNode		<op> ++
			PostDecrement,				// UnaryExprNode		<op> --
			Plus,						// UnaryExprNode		+ <op>
			Minus,						// UnaryExprNode		- <op>
			Complement,					// UnaryExprNode		~ <op>
			Not,						// UnaryExprNode		! <op>

			Add,						// BinaryExprNode		<op1> + <op2>
			Subtract,					// BinaryExprNode		<op1> - <op2>
			Multiply,					// BinaryExprNode		<op1> * <op2>
			Divide,						// BinaryExprNode		<op1> / <op2>
			Modulo,						// BinaryExprNode		<op1> % <op2>
			LeftShift,					// BinaryExprNode		<op1> << <op2>
			RightShift,					// BinaryExprNode		<op1> >> <op2>
			LogicalRightShift,			// BinaryExprNode		<op1> >>> <op2>
			And,						// BinaryExprNode		<op1> & <op2>
			Xor,						// BinaryExprNode		<op1> ^ <op2>
			Or,							// BinaryExprNode		<op1> | <op2>
			LogicalAnd,					// BinaryExprNode		<op1> && <op2>
			LogicalXor,					// BinaryExprNode		<op1> ^^ <op2>
			LogicalOr,					// BinaryExprNode		<op1> || <op2>

			Equal,						// BinaryExprNode		<op1> == <op2>
			NotEqual,					// BinaryExprNode		<op1> != <op2>
			LessThan,					// BinaryExprNode		<op1> < <op2>
			LessThanOrEqual,			// BinaryExprNode		<op1> <= <op2>
			GreaterThan,				// BinaryExprNode		<op1> > <op2>
			GreaterThanOrEqual,			// BinaryExprNode		<op1> >= <op2>
			Identical,					// BinaryExprNode		<op1> === <op2>
			NotIdentical,				// BinaryExprNode		<op1> !== <op2>
			In,							// BinaryExprNode		<op1> in <op2>
			Instanceof,					// BinaryExprNode		<op1> instanceof <op2>

			Assignment,					// BinaryExprNode		<op1> = <op2>
			AddEquals,					// BinaryExprNode		<op1> += <op2>
			SubtractEquals,				// BinaryExprNode		<op1> -= <op2>
			MultiplyEquals,				// BinaryExprNode		<op1> *= <op2>
			DivideEquals,				// BinaryExprNode		<op1> /= <op2>
			ModuloEquals,				// BinaryExprNode		<op1> %= <op2>
			LeftShiftEquals,			// BinaryExprNode		<op1> <<= <op2>
			RightShiftEquals,			// BinaryExprNode		<op1> >>= <op2>
			LogicalRightShiftEquals,	// BinaryExprNode		<op1> >>>= <op2>
			AndEquals,					// BinaryExprNode		<op1> &= <op2>
			XorEquals,					// BinaryExprNode		<op1> ^= <op2>
			OrEquals,					// BinaryExprNode		<op1> |= <op2>
			LogicalAndEquals,			// BinaryExprNode		<op1> &&= <op2>
			LogicalXorEquals,			// BinaryExprNode		<op1> ^^= <op2>
			LogicalOrEquals,			// BinaryExprNode		<op1> ||= <op2>

			Conditional,				// TernaryExprNode		<op1> ? <op2> : <op3>
			Comma						// BinaryExprNode		<op1> , <op2>	// Comma expressions only
		};
		
		Kind kind;						// The node's kind

		ExprNode(const SourcePosition &pos, Kind kind): ParseNode(pos), kind(kind) {}
	};

	struct IdentifierExprNode: ExprNode {
		const StringAtom &identifier;	// The identifier

		IdentifierExprNode(const SourcePosition &pos, Kind kind, const StringAtom &identifier):
				ExprNode(pos, kind), identifier(identifier) {}
	};
	
	struct OpIdentifierExprNode: IdentifierExprNode {
		ExprNode &op;					// The namespace expression or indexed expression

		OpIdentifierExprNode(const SourcePosition &pos, Kind kind, const StringAtom &identifier, ExprNode &op):
				IdentifierExprNode(pos, kind, identifier), op(op) {}
	};
	
	struct NumberExprNode: ExprNode {
		float64 value;					// The number's value

		NumberExprNode(const SourcePosition &pos, float64 value): ExprNode(pos, Num), value(value) {}
	};
	
	struct StringExprNode: ExprNode {
		String &str;					// The string

		StringExprNode(const SourcePosition &pos, Kind kind, String &str): ExprNode(pos, kind), str(str) {}
	};
	
	struct RegExpExprNode: ExprNode {
		const StringAtom &regExp;		// The regular expression's contents
		String &flags;					// The regular expression's flags

		RegExpExprNode(const SourcePosition &pos, Kind kind, const StringAtom &regExp, String &flags):
				ExprNode(pos, kind), regExp(regExp), flags(flags) {}
	};
	
	struct NumUnitExprNode: StringExprNode { // str is the unit string
		String &numStr;					// The number's source string
		float64 num;					// The number's value

		NumUnitExprNode(const SourcePosition &pos, Kind kind, String &unitStr, String &numStr, float64 num):
				StringExprNode(pos, kind, unitStr), numStr(numStr), num(num) {}
	};
	
	struct ExprNodeUnitExprNode: StringExprNode { // str is the unit string
		ExprNode &op;					// The expression to which the unit is applied

		ExprNodeUnitExprNode(const SourcePosition &pos, Kind kind, String &unitStr, ExprNode &op):
				StringExprNode(pos, kind, unitStr), op(op) {}
	};

	struct FunctionExprNode: ExprNode {
		FunctionDefinition function;	// Function definition

		FunctionExprNode(const SourcePosition &pos, Kind kind): ExprNode(pos, kind) {}
	};
	
	struct ExprList: ArenaObject {
		ExprList *next;					// Next expression in linked list
		ExprNode &expr;					// Attribute expression
		
		explicit ExprList(ExprNode &expr): next(0), expr(expr) {}
	};
	
	struct ExprPairList: ArenaObject {
		ExprPairList *next;				// Next pair in linked list
		ExprNode *field;				// Field expression or nil if not provided
		ExprNode *value;				// Value expression or nil if not provided
		
		explicit ExprPairList(ExprNode *field = 0, ExprNode *value = 0): next(0), field(field), value(value) {}
	};

	struct PairListExprNode: ExprNode {
		ExprPairList *pairs;			// Linked list of pairs

		PairListExprNode(const SourcePosition &pos, Kind kind, ExprPairList *pairs): ExprNode(pos, kind), pairs(pairs) {}
	};

	struct InvokeExprNode: PairListExprNode {
		ExprNode &op;					// The called function, called constructor, or indexed object

		InvokeExprNode(const SourcePosition &pos, Kind kind, ExprPairList *pairs, ExprNode &op):
				PairListExprNode(pos, kind, pairs), op(op) {}
	};

	struct UnaryExprNode: ExprNode {
		ExprNode &op;					// The unary operator's operand

		UnaryExprNode(const SourcePosition &pos, Kind kind, ExprNode &op): ExprNode(pos, kind), op(op) {}
	};

	struct BinaryExprNode: ExprNode {
		ExprNode &op1;					// The binary operator's first operand
		ExprNode &op2;					// The binary operator's second operand

		BinaryExprNode(const SourcePosition &pos, Kind kind, ExprNode &op1, ExprNode &op2):
				ExprNode(pos, kind), op1(op1), op2(op2) {}
	};

	struct TernaryExprNode: ExprNode {
		ExprNode &op1;					// The ternary operator's first operand
		ExprNode &op2;					// The ternary operator's second operand
		ExprNode &op3;					// The ternary operator's third operand

		TernaryExprNode(const SourcePosition &pos, Kind kind, ExprNode &op1, ExprNode &op2, ExprNode &op3):
				ExprNode(pos, kind), op1(op1), op2(op2), op3(op3) {}
	};



	struct StmtNode: ParseNode {
		enum Kind {						// Actual class			Operands
			Empty,						// StmtNode				;
			Expression,					// ExprStmtNode			<expr> ;
			Block,						// BlockStmtNode		<attributes> { <statements> }
			Label,						// LabelStmtNode		<label> : <stmt>
			If,							// UnaryStmtNode		if ( <expr> ) <stmt>
			IfElse,						// BinaryStmtNode		if ( <expr> ) <stmt> else <stmt2>
			Switch,						// SwitchStmtNode		switch ( <expr> ) <statements>
			While,						// UnaryStmtNode		while ( <expr> ) <stmt>
			DoWhile,					// UnaryStmtNode		do <stmt> while ( <expr> )
			With,						// UnaryStmtNode		with ( <expr> ) <stmt>
			For,						// ForStmtNode			for ( <initializer> ; <expr2> ; <expr3> ) <stmt>
			ForExprIn,					// ForExprInStmtNode	for ( <varExpr> in <container> ) <stmt>
			ForConstIn,					// ForVarInStmtNode		for ( const <binding> in <container> ) <stmt>
			ForVarIn,					// ForVarInStmtNode		for ( var <binding> in <container> ) <stmt>
			Case,						// ExprStmtNode			case <expr> :	or   default :	// Only occurs directly inside a Switch
			Break,						// GoStmtNode			break ;   or   break <label> ;
			Continue,					// GoStmtNode			continue ;   or   continue <label> ;
			Return,						// ExprStmtNode			return ;   or   return <expr> ;
			Throw,						// ExprStmtNode			throw <expr> ;
			Try,						// TryStmtNode			try <stmt> <catches> <finally>
			Const,						// VariableStmtNode		<attributes> const <bindings> ;
			Var,						// VariableStmtNode		<attributes> var <bindings> ;
			Function,					// FunctionStmtNode		<attributes> function <function>
			Class,						// ClassStmtNode		<attributes> class <identifier> extends <superclasses> <body>
			Language					// LanguageStmtNode		language <language> ;
		};
		
		Kind kind;						// The node's kind
		StmtNode *next;					// Next statement in a linked list of statements in this block

		StmtNode(const SourcePosition &pos, Kind kind): ParseNode(pos), kind(kind), next(0) {}
	};

	struct ExprStmtNode: StmtNode {
		ExprNode *expr;					// The expression statement's expression.  May be null for default: or return-with-no-expression statements.

		ExprStmtNode(const SourcePosition &pos, Kind kind, ExprNode *expr): StmtNode(pos, kind), expr(expr) {}
	};

	struct AttributeStmtNode: StmtNode {
		ExprList *attributes;			// Linked list of block or definition's attributes

		AttributeStmtNode(const SourcePosition &pos, Kind kind, ExprList *attributes): StmtNode(pos, kind), attributes(attributes) {}
	};
	
	struct BlockStmtNode: AttributeStmtNode {
		StmtNode *statements;			// Linked list of block's statements

		BlockStmtNode(const SourcePosition &pos, Kind kind, ExprList *attributes): AttributeStmtNode(pos, kind, attributes) {}
	};
	
	struct LabelStmtNode: StmtNode {
		const StringAtom &label;		// The label
		StmtNode &stmt;					// Labeled statement

		LabelStmtNode(const SourcePosition &pos, Kind kind, const StringAtom &label, StmtNode &stmt):
				StmtNode(pos, kind), label(label), stmt(stmt) {}
	};
	
	struct UnaryStmtNode: ExprStmtNode {
		StmtNode &stmt;					// First substatement

		UnaryStmtNode(const SourcePosition &pos, Kind kind, ExprNode *expr, StmtNode &stmt):
				ExprStmtNode(pos, kind, expr), stmt(stmt) {}
	};
	
	struct BinaryStmtNode: UnaryStmtNode {
		StmtNode &stmt2;				// Second substatement

		BinaryStmtNode(const SourcePosition &pos, Kind kind, ExprNode *expr, StmtNode &stmt1, StmtNode &stmt2):
				UnaryStmtNode(pos, kind, expr, stmt1), stmt2(stmt2) {}
	};
	
	struct ForStmtNode: StmtNode {
		StmtNode *initializer;			// First item in parentheses; either nil (if not provided), an Expression, or a Var, or a Const.
		ExprNode *expr2;				// Second item in parentheses; nil if not provided
		ExprNode *expr3;				// Third item in parentheses; nil if not provided
		StmtNode &stmt;					// Substatement

		ForStmtNode(const SourcePosition &pos, Kind kind, StmtNode &stmt):
				StmtNode(pos, kind), stmt(stmt) {}
	};
	
	struct ForInStmtNode: StmtNode {
		ExprNode &container;			// Subexpression after 'in'
		StmtNode &stmt;					// Substatement

		ForInStmtNode(const SourcePosition &pos, Kind kind, ExprNode &container, StmtNode &stmt):
				StmtNode(pos, kind), container(container), stmt(stmt) {}
	};
	
	struct ForExprInStmtNode: ForInStmtNode {
		ExprNode &varExpr;				// Subexpression before 'in'

		ForExprInStmtNode(const SourcePosition &pos, Kind kind, ExprNode &container, StmtNode &stmt, ExprNode &varExpr):
				ForInStmtNode(pos, kind, container, stmt), varExpr(varExpr) {}
	};
	
	struct ForVarInStmtNode: ForInStmtNode {
		VariableBinding &binding;		// Var or const binding before 'in'

		ForVarInStmtNode(const SourcePosition &pos, Kind kind, ExprNode &container, StmtNode &stmt, VariableBinding &binding):
				ForInStmtNode(pos, kind, container, stmt), binding(binding) {}
	};
	
	struct SwitchStmtNode: ExprStmtNode {
		StmtNode *statements;			// Linked list of switch block's statements, which may include Case and Default statements

		SwitchStmtNode(const SourcePosition &pos, Kind kind, ExprNode *expr):
				ExprStmtNode(pos, kind, expr) {}
	};
	
	struct GoStmtNode: StmtNode {
		const StringAtom *label;		// The label; nil if none

		GoStmtNode(const SourcePosition &pos, Kind kind, const StringAtom *label): StmtNode(pos, kind), label(label) {}
	};
	
	struct CatchClause: ParseNode {
		CatchClause *next;				// Next catch clause in a linked list of catch clauses
		const StringAtom &identifier;	// The name of the variable that will hold the exception
		ExprNode *type;					// Type expression or nil if not provided
		StmtNode &stmt;					// The catch clause's body

		CatchClause(const SourcePosition &pos, const StringAtom &identifier, ExprNode *type, StmtNode &stmt):
				ParseNode(pos), next(0), identifier(identifier), type(type), stmt(stmt) {}
	};
	
	struct TryStmtNode: StmtNode {
		StmtNode &stmt;					// Substatement being tried; usually a block
		CatchClause *catches;			// Linked list of catch blocks; may be nil
		StmtNode *finally;				// Finally block or nil if none

		TryStmtNode(const SourcePosition &pos, Kind kind, StmtNode &stmt, CatchClause *catches, StmtNode *finally):
				StmtNode(pos, kind), stmt(stmt), catches(catches), finally(finally) {}
	};

	struct VariableStmtNode: AttributeStmtNode {
		VariableBinding *bindings;		// Linked list of variable bindings

		VariableStmtNode(const SourcePosition &pos, Kind kind, ExprList *attributes): AttributeStmtNode(pos, kind, attributes) {}
	};

	struct FunctionStmtNode: AttributeStmtNode {
		FunctionDefinition function;	// Function definition

		FunctionStmtNode(const SourcePosition &pos, Kind kind, ExprList *attributes): AttributeStmtNode(pos, kind, attributes) {}
	};

	struct ClassStmtNode: AttributeStmtNode {
		const StringAtom &identifier;	// The class's name
		ExprList *superclasses;			// Linked list of superclass expressions
		StmtNode &body;					// The class's body

		ClassStmtNode(const SourcePosition &pos, Kind kind, ExprList *attributes, const StringAtom &identifier, ExprList *superclasses,
					  StmtNode &body):
				AttributeStmtNode(pos, kind, attributes), identifier(identifier), superclasses(superclasses), body(body) {}
	};

	struct LanguageStmtNode: StmtNode {
		JavaScript::Language language;	// The selected language

		LanguageStmtNode(const SourcePosition &pos, Kind kind, JavaScript::Language language):
				StmtNode(pos, kind), language(language) {}
	};


	//class Parser: public Lexer {
	//};
}
#endif
