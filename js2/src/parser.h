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
#include <vector>

namespace JavaScript {

	class StringAtom;
	class World;

//
// Reader
//

	// A Reader reads Unicode characters from a source string.
	// Calling get() yields all of the characters in sequence.  One character may be read
	// past the end of the source; that character appears as a null.
	class Reader {
		const char16 *begin;			// Beginning of source text
		const char16 *p;				// Position in source text
		const char16 *end;				// End of source text; *end is a null character
	  public:
		const String source;			// Source text
		const String sourceLocation;	// Description of location from which the source text came
	  private:
		const uint32 initialLineNum;	// One-based number of current line
		std::vector<const char16 *> linePositions; // Array of line starts recorded by beginLine()

		String *recordString;			// String, if any, into which recordChar() records characters; not owned by the Reader
		const char16 *recordBase;		// Position of last beginRecording() call
		const char16 *recordPos;		// Position of last recordChar() call; nil if a discrepancy occurred
	  public:
		
		Reader(const String &source, const String &sourceLocation, uint32 initialLineNum = 1);
	  private:
	    Reader(const Reader&);			// No copy constructor
	    void operator=(const Reader&);	// No assignment operator
	  public:

		char16 get() {ASSERT(p <= end); return *p++;}
		char16 peek() {ASSERT(p <= end); return *p;}
		void unget(uint32 n = 1) {ASSERT(p >= begin + n); p -= n;}
		uint32 getPos() const {return static_cast<uint32>(p - begin);}
		void setPos(uint32 pos) {ASSERT(pos <= getPos()); p = begin + pos;}

		bool eof() const {ASSERT(p <= end); return p == end;}
		bool peekEof(char16 ch) const {ASSERT(p <= end && *p == ch); return !ch && p == end;} // Faster version.  ch is the result of a peek
		bool pastEof() const {return p == end+1;}
		bool getEof(char16 ch) const {ASSERT(p[-1] == ch); return !ch && p == end+1;} // Faster version.  ch is the result of a get

		void beginLine();
		uint32 posToLineNum(uint32 pos) const;
		uint32 getLine(uint32 lineNum, const char16 *&lineBegin, const char16 *&lineEnd) const;

		void beginRecording(String &recordString);
		void recordChar(char16 ch);
		String &endRecording();
		
		void error(Exception::Kind kind, const String &message, uint32 pos);
	};



//
// Lexer
//

	class Token {
		static const char *const kindNames[];
	  public:
		enum Kind {	// Keep synchronized with kindNames table
		  // Special
			end,						// End of token stream

			number,						// Number
			string,						// String
			unit,						// Unit after number
			regExp,						// Regular expression

		  // Punctuators
			openParenthesis,			// (
			closeParenthesis,			// )
			openBracket,				// [
			closeBracket,				// ]
			openBrace,					// {
			closeBrace,					// }

			comma,						// ,
			semicolon,					// ;
			dot,						// .
			doubleDot,					// ..
			tripleDot,					// ...
			arrow,						// ->
			colon,						// :
			doubleColon,				// ::
			pound,						// #
			at,							// @

			increment,					// ++
			decrement,					// --

			complement,					// ~
			logicalNot,					// !

			times,						// *
			divide,						// /
			modulo,						// %
			plus,						// +
			minus,						// -
			leftShift,					// <<
			rightShift,					// >>
			logicalRightShift,			// >>>
			logicalAnd,					// &&
			logicalXor,					// ^^
			logicalOr,					// ||
			bitwiseAnd,					// &	// These must be at constant offsets from logicalAnd ... logicalOr
			bitwiseXor,					// ^
			bitwiseOr,					// |

			assignment,					// =
			timesEquals,				// *=	// These must be at constant offsets from times ... bitwiseOr
			divideEquals,				// /=
			moduloEquals,				// %=
			plusEquals,					// +=
			minusEquals,				// -=
			leftShiftEquals,			// <<=
			rightShiftEquals,			// >>=
			logicalRightShiftEquals,	// >>>=
			logicalAndEquals,			// &&=
			logicalXorEquals,			// ^^=
			logicalOrEquals,			// ||=
			bitwiseAndEquals,			// &=
			bitwiseXorEquals,			// ^=
			bitwiseOrEquals,			// |=

			equal,						// ==
			notEqual,					// !=
			lessThan,					// <
			lessThanOrEqual,			// <=
			greaterThan,				// >	// >, >= must be at constant offsets from <, <=
			greaterThanOrEqual,			// >=
			identical,					// ===
			notIdentical,				// !==

			question,					// ?

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
			
			identifier,					// Non-keyword identifier (may be same as a keyword if it contains an escape code)

			KindsEnd,					// End of token kinds
			KeywordsBegin = Abstract,	// Beginning of range of special identifier tokens
			KeywordsEnd = identifier,	// End of range of special identifier tokens
			NonReservedBegin = Box,		// Beginning of range of non-reserved words
			NonReservedEnd = identifier,// End of range of non-reserved words
			KindsWithCharsBegin = number,// Beginning of range of tokens for which the chars field (below) is valid
			KindsWithCharsEnd = regExp+1// End of range of tokens for which the chars field (below) is valid
		};

#define CASE_TOKEN_NONRESERVED	\
		 Token::Box:			\
	case Token::Constructor:	\
	case Token::Field:			\
	case Token::Get:			\
	case Token::Language:		\
	case Token::Local:			\
	case Token::Method:			\
	case Token::Override:		\
	case Token::Set:			\
	case Token::Version:		\
	case Token::identifier

	  private:
	  #ifdef DEBUG
		bool valid;						// True if this token has been initialized
	  #endif
		Kind kind;						// The token's kind
		bool lineBreak;					// True if line break precedes this token
		uint32 pos;						// Source position of this token
		const StringAtom *id;			// The token's characters; non-nil for identifiers, keywords, and regular expressions only
		String chars;					// The token's characters; valid for strings, units, numbers, and regular expression flags only
		float64 value;					// The token's value (numbers only)
		
	  #ifdef DEBUG
		Token(): valid(false) {}
	  #endif

	  public:
		static void initKeywords(World &world);
		static bool isIdentifierKind(Kind kind) {ASSERT(NonReservedEnd == identifier && KindsEnd == identifier+1); return kind >= NonReservedBegin;}
		static bool isSpecialKind(Kind kind) {return kind <= regExp || kind == identifier;}
		static const char *kindName(Kind kind) {ASSERT(uint(kind) < KindsEnd); return kindNames[kind];}

		Kind getKind() const {ASSERT(valid); return kind;}
		bool hasKind(Kind k) const {ASSERT(valid); return kind == k;}
		bool getLineBreak() const {ASSERT(valid); return lineBreak;}
		uint32 getPos() const {ASSERT(valid); return pos;}
		const StringAtom &getIdentifier() const {ASSERT(valid && id); return *id;}
		const String &getChars() const {ASSERT(valid && kind >= KindsWithCharsBegin && kind < KindsWithCharsEnd); return chars;}
		float64 getValue() const {ASSERT(valid && kind == number); return value;}

		friend String &operator+=(String &s, Kind k) {return s += kindName(k);}
		friend String &operator+=(String &s, const Token &t) {t.print(s); return s;}
		void print(String &dst, bool debug = false) const;
		
		friend class Lexer;
	};


	class Lexer {
		enum {tokenLookahead = 2};		// Number of tokens that can be simultaneously live
	  #ifdef DEBUG
		enum {tokenGuard = 10};			// Number of invalid tokens added to circular token buffer to catch references to old tokens
	  #else
		enum {tokenGuard = 0};			// Number of invalid tokens added to circular token buffer to catch references to old tokens
	  #endif
		enum {tokenBufferSize = tokenLookahead + tokenGuard}; // Token lookahead buffer size

		Token tokens[tokenBufferSize];	// Circular buffer of recently read or lookahead tokens
		Token *nextToken;				// Address of next Token in the circular buffer to be returned by get()
		int nTokensFwd;					// Net number of Tokens on which unget() has been called; these Tokens are ahead of nextToken
	  #ifdef DEBUG
		int nTokensBack;				// Number of Tokens on which unget() can be called; these Tokens are beind nextToken
		bool savedPreferRegExp[tokenBufferSize]; // Circular buffer of saved values of preferRegExp to get() calls
	  #endif
		bool lexingUnit;				// True if lexing a unit identifier immediately following a number
	  public:
		World &world;
		Reader reader;

		Lexer(World &world, const String &source, const String &sourceLocation, uint32 initialLineNum = 1);
		
		const Token &get(bool preferRegExp);
		const Token *eat(bool preferRegExp, Token::Kind kind);
		const Token &peek(bool preferRegExp);
		void redesignate(bool preferRegExp);
		void unget();
		uint32 getPos() const;

	  private:
		void syntaxError(const char *message, uint backUp = 1);
		char16 getChar();
		char16 internalGetChar(char16 ch);
		char16 peekChar();
		char16 internalPeekChar(char16 ch);
		bool testChar(char16 ch);

		char16 lexEscape(bool unicodeOnly);
		bool lexIdentifier(String &s, bool allowLeadingDigit);
		bool lexNumeral();
		void lexString(String &s, char16 separator);
		void lexRegExp();
		void lexToken(bool preferRegExp);
	};

  #ifndef DEBUG
	inline void Lexer::redesignate(bool) {}
  #endif

	// Return the position of the first character of the next token, which must have been peeked.
	inline uint32 Lexer::getPos() const
	{
		ASSERT(nTokensFwd);
		return nextToken->getPos();
	}


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
		uint32 pos;						// Position of this statement or expression
		
		explicit ParseNode(uint32 pos): pos(pos) {}
	};

	// A helper template for creating linked lists of ParseNode subtypes.  N should be derived
	// from a ParseNode and should have an instance variable called <next> of type N* and that
	// is initialized to nil when an N instance is created.
	template<class N>
	class NodeQueue {
	  public:
		N *first;						// Head of queue
	  private:
		N **last;						// Next link of last element of queue
		
	  public:
		NodeQueue(): first(0), last(&first) {}
	  private:
	    NodeQueue(const NodeQueue&);		// No copy constructor
	    void operator=(const NodeQueue&);	// No assignment operator
	  public:
		void operator+=(N *elt) {ASSERT(elt && !elt->next); *last = elt; last = &elt->next;}
	};


	struct FunctionName {
		enum Prefix {
			normal,						// No prefix
			Get,						// get
			Set,						// set
			New							// new
		};
		
		Prefix prefix;					// The name's prefix, if any
		const StringAtom *name;			// The name; nil if omitted
		
		FunctionName(): prefix(normal), name(0) {}
	};
	
	struct ExprNode;
	struct StmtNode;
	
	struct VariableBinding: ParseNode {
		VariableBinding *next;			// Next binding in a linked list of variable or parameter bindings
		const StringAtom *name;			// The variable's name; nil if omitted, which currently can only happen for ... parameters
		ExprNode *type;					// Type expression or nil if not provided
		ExprNode *initializer;			// Initial value expression or nil if not provided
		
		VariableBinding(uint32 pos): ParseNode(pos), next(0), name(0), type(0), initializer(0) {}
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
			identifier,					// IdentifierExprNode	<name>
			number,						// NumberExprNode		<value>
			string,						// StringExprNode		<str>
			regExp,						// RegExpExprNode		/<regExp>/<flags>
			Null,						// ExprNode				null
			True,						// ExprNode				true
			False,						// ExprNode				false
			This,						// ExprNode				this
			Super,						// ExprNode				super

			parentheses,				// UnaryExprNode		(<op>)
			numUnit,					// NumUnitExprNode		<num> "<str>"   or   <num><str>
			exprUnit,					// ExprUnitExprNode		(<op>) "<str>"
			qualifiedIdentifier,		// OpIdentifierExprNode	<op> :: <name>
			
			objectLiteral,				// PairListExprNode		{<field>:<value>, <field>:<value>, ..., <field>:<value>}
			arrayLiteral,				// PairListExprNode		[<value>, <value>, ..., <value>]
			functionLiteral,			// FunctionExprNode		function <function>

			call,						// InvokeExprNode		<op>(<field>:<value>, <field>:<value>, ..., <field>:<value>)
			New,						// InvokeExprNode		new <op>(<field>:<value>, <field>:<value>, ..., <field>:<value>)
			index,						// InvokeExprNode		<op>[<field>:<value>, <field>:<value>, ..., <field>:<value>]

			dot,						// BinaryExprNode		<op1> . <op2>  // <op2> must be identifier or qualifiedIdentifier
			dotParen,					// BinaryExprNode		<op1> .( <op2> )
			at,							// BinaryExprNode		<op1> @ <op2>   or   <op1> @( <op2> )

			Delete,						// UnaryExprNode		delete <op>
			Typeof,						// UnaryExprNode		typeof <op>
			Eval,						// UnaryExprNode		eval <op>
			preIncrement,				// UnaryExprNode		++ <op>
			preDecrement,				// UnaryExprNode		-- <op>
			postIncrement,				// UnaryExprNode		<op> ++
			postDecrement,				// UnaryExprNode		<op> --
			plus,						// UnaryExprNode		+ <op>
			minus,						// UnaryExprNode		- <op>
			complement,					// UnaryExprNode		~ <op>
			logicalNot,					// UnaryExprNode		! <op>

			add,						// BinaryExprNode		<op1> + <op2>
			subtract,					// BinaryExprNode		<op1> - <op2>
			multiply,					// BinaryExprNode		<op1> * <op2>
			divide,						// BinaryExprNode		<op1> / <op2>
			modulo,						// BinaryExprNode		<op1> % <op2>
			leftShift,					// BinaryExprNode		<op1> << <op2>
			rightShift,					// BinaryExprNode		<op1> >> <op2>
			logicalRightShift,			// BinaryExprNode		<op1> >>> <op2>
			bitwiseAnd,					// BinaryExprNode		<op1> & <op2>
			bitwiseXor,					// BinaryExprNode		<op1> ^ <op2>
			bitwiseOr,					// BinaryExprNode		<op1> | <op2>
			logicalAnd,					// BinaryExprNode		<op1> && <op2>
			logicalXor,					// BinaryExprNode		<op1> ^^ <op2>
			logicalOr,					// BinaryExprNode		<op1> || <op2>

			equal,						// BinaryExprNode		<op1> == <op2>
			notEqual,					// BinaryExprNode		<op1> != <op2>
			lessThan,					// BinaryExprNode		<op1> < <op2>
			lessThanOrEqual,			// BinaryExprNode		<op1> <= <op2>
			greaterThan,				// BinaryExprNode		<op1> > <op2>
			greaterThanOrEqual,			// BinaryExprNode		<op1> >= <op2>
			identical,					// BinaryExprNode		<op1> === <op2>
			notIdentical,				// BinaryExprNode		<op1> !== <op2>
			In,							// BinaryExprNode		<op1> in <op2>
			Instanceof,					// BinaryExprNode		<op1> instanceof <op2>

			assignment,					// BinaryExprNode		<op1> = <op2>
			addEquals,					// BinaryExprNode		<op1> += <op2>
			subtractEquals,				// BinaryExprNode		<op1> -= <op2>
			multiplyEquals,				// BinaryExprNode		<op1> *= <op2>
			divideEquals,				// BinaryExprNode		<op1> /= <op2>
			moduloEquals,				// BinaryExprNode		<op1> %= <op2>
			leftShiftEquals,			// BinaryExprNode		<op1> <<= <op2>
			rightShiftEquals,			// BinaryExprNode		<op1> >>= <op2>
			logicalRightShiftEquals,	// BinaryExprNode		<op1> >>>= <op2>
			bitwiseAndEquals,			// BinaryExprNode		<op1> &= <op2>
			bitwiseXorEquals,			// BinaryExprNode		<op1> ^= <op2>
			bitwiseOrEquals,			// BinaryExprNode		<op1> |= <op2>
			logicalAndEquals,			// BinaryExprNode		<op1> &&= <op2>
			logicalXorEquals,			// BinaryExprNode		<op1> ^^= <op2>
			logicalOrEquals,			// BinaryExprNode		<op1> ||= <op2>

			conditional,				// TernaryExprNode		<op1> ? <op2> : <op3>
			comma						// BinaryExprNode		<op1> , <op2>	// Comma expressions only
		};
		
	  private:
		Kind kind;						// The node's kind
	  public:

		ExprNode(uint32 pos, Kind kind): ParseNode(pos), kind(kind) {}

		Kind getKind() const {return kind;}
		bool hasKind(Kind k) const {return kind == k;}

		static bool isFieldKind(Kind kind) {return kind == identifier || kind == number || kind == string || kind == qualifiedIdentifier;}
	};

	struct IdentifierExprNode: ExprNode {
		const StringAtom &name;			// The identifier

		IdentifierExprNode(uint32 pos, Kind kind, const StringAtom &name):
				ExprNode(pos, kind), name(name) {}
	};
	
	struct OpIdentifierExprNode: IdentifierExprNode {
		ExprNode *op;					// The namespace expression or indexed expression; non-nil only

		OpIdentifierExprNode(uint32 pos, Kind kind, const StringAtom &name, ExprNode *op):
				IdentifierExprNode(pos, kind, name), op(op) {ASSERT(op);}
	};
	
	struct NumberExprNode: ExprNode {
		float64 value;					// The number's value

		NumberExprNode(uint32 pos, float64 value): ExprNode(pos, number), value(value) {}
	};
	
	struct StringExprNode: ExprNode {
		String &str;					// The string

		StringExprNode(uint32 pos, Kind kind, String &str): ExprNode(pos, kind), str(str) {}
	};
	
	struct RegExpExprNode: ExprNode {
		const StringAtom &regExp;		// The regular expression's contents
		String &flags;					// The regular expression's flags

		RegExpExprNode(uint32 pos, Kind kind, const StringAtom &regExp, String &flags):
				ExprNode(pos, kind), regExp(regExp), flags(flags) {}
	};
	
	struct NumUnitExprNode: StringExprNode { // str is the unit string
		String &numStr;					// The number's source string
		float64 num;					// The number's value

		NumUnitExprNode(uint32 pos, Kind kind, String &numStr, float64 num, String &unitStr):
				StringExprNode(pos, kind, unitStr), numStr(numStr), num(num) {}
	};
	
	struct ExprUnitExprNode: StringExprNode { // str is the unit string
		ExprNode *op;					// The expression to which the unit is applied; non-nil only

		ExprUnitExprNode(uint32 pos, Kind kind, ExprNode *op, String &unitStr):
				StringExprNode(pos, kind, unitStr), op(op) {ASSERT(op);}
	};

	struct FunctionExprNode: ExprNode {
		FunctionDefinition function;	// Function definition

		FunctionExprNode(uint32 pos, Kind kind): ExprNode(pos, kind) {}
	};
	
	struct ExprList: ArenaObject {
		ExprList *next;					// Next expression in linked list
		ExprNode *expr;					// Attribute expression; non-nil only
		
		explicit ExprList(ExprNode *expr): next(0), expr(expr) {ASSERT(expr);}
	};
	
	struct ExprPairList: ArenaObject {
		ExprPairList *next;				// Next pair in linked list
		ExprNode *field;				// Field expression or nil if not provided
		ExprNode *value;				// Value expression or nil if not provided
		
		explicit ExprPairList(ExprNode *field = 0, ExprNode *value = 0): next(0), field(field), value(value) {}
	};

	struct PairListExprNode: ExprNode {
		ExprPairList *pairs;			// Linked list of pairs

		PairListExprNode(uint32 pos, Kind kind, ExprPairList *pairs): ExprNode(pos, kind), pairs(pairs) {}
	};

	struct InvokeExprNode: PairListExprNode {
		ExprNode *op;					// The called function, called constructor, or indexed object; non-nil only

		InvokeExprNode(uint32 pos, Kind kind, ExprNode *op, ExprPairList *pairs):
				PairListExprNode(pos, kind, pairs), op(op) {ASSERT(op);}
	};

	struct UnaryExprNode: ExprNode {
		ExprNode *op;					// The unary operator's operand; non-nil only

		UnaryExprNode(uint32 pos, Kind kind, ExprNode *op): ExprNode(pos, kind), op(op) {ASSERT(op);}
	};

	struct BinaryExprNode: ExprNode {
		ExprNode *op1;					// The binary operator's first operand; non-nil only
		ExprNode *op2;					// The binary operator's second operand; non-nil only

		BinaryExprNode(uint32 pos, Kind kind, ExprNode *op1, ExprNode *op2):
				ExprNode(pos, kind), op1(op1), op2(op2) {ASSERT(op1 && op2);}
	};

	struct TernaryExprNode: ExprNode {
		ExprNode *op1;					// The ternary operator's first operand; non-nil only
		ExprNode *op2;					// The ternary operator's second operand; non-nil only
		ExprNode *op3;					// The ternary operator's third operand; non-nil only

		TernaryExprNode(uint32 pos, Kind kind, ExprNode *op1, ExprNode *op2, ExprNode *op3):
				ExprNode(pos, kind), op1(op1), op2(op2), op3(op3) {ASSERT(op1 && op2 && op3);}
	};



	struct StmtNode: ParseNode {
		enum Kind {						// Actual class			Operands
			empty,						// StmtNode				;
			expression,					// ExprStmtNode			<expr> ;
			block,						// BlockStmtNode		<attributes> { <statements> }
			label,						// LabelStmtNode		<label> : <stmt>
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
			Class,						// ClassStmtNode		<attributes> class <name> extends <superclasses> <body>
			Language					// LanguageStmtNode		language <language> ;
		};
		
		Kind kind;						// The node's kind
		StmtNode *next;					// Next statement in a linked list of statements in this block

		StmtNode(uint32 pos, Kind kind): ParseNode(pos), kind(kind), next(0) {}
	};

	struct ExprStmtNode: StmtNode {
		ExprNode *expr;					// The expression statement's expression.  May be nil for default: or return-with-no-expression statements.

		ExprStmtNode(uint32 pos, Kind kind, ExprNode *expr): StmtNode(pos, kind), expr(expr) {}
	};

	struct AttributeStmtNode: StmtNode {
		ExprList *attributes;			// Linked list of block or definition's attributes

		AttributeStmtNode(uint32 pos, Kind kind, ExprList *attributes): StmtNode(pos, kind), attributes(attributes) {}
	};
	
	struct BlockStmtNode: AttributeStmtNode {
		StmtNode *statements;			// Linked list of block's statements

		BlockStmtNode(uint32 pos, Kind kind, ExprList *attributes): AttributeStmtNode(pos, kind, attributes) {}
	};
	
	struct LabelStmtNode: StmtNode {
		const StringAtom &label;		// The label
		StmtNode *stmt;					// Labeled statement; non-nil only

		LabelStmtNode(uint32 pos, Kind kind, const StringAtom &label, StmtNode *stmt):
				StmtNode(pos, kind), label(label), stmt(stmt) {ASSERT(stmt);}
	};
	
	struct UnaryStmtNode: ExprStmtNode {
		StmtNode *stmt;					// First substatement; non-nil only

		UnaryStmtNode(uint32 pos, Kind kind, ExprNode *expr, StmtNode *stmt):
				ExprStmtNode(pos, kind, expr), stmt(stmt) {ASSERT(stmt);}
	};
	
	struct BinaryStmtNode: UnaryStmtNode {
		StmtNode *stmt2;				// Second substatement; non-nil only

		BinaryStmtNode(uint32 pos, Kind kind, ExprNode *expr, StmtNode *stmt1, StmtNode *stmt2):
				UnaryStmtNode(pos, kind, expr, stmt1), stmt2(stmt2) {ASSERT(stmt2);}
	};
	
	struct ForStmtNode: StmtNode {
		StmtNode *initializer;			// First item in parentheses; either nil (if not provided), an expression, or a Var, or a Const.
		ExprNode *expr2;				// Second item in parentheses; nil if not provided
		ExprNode *expr3;				// Third item in parentheses; nil if not provided
		StmtNode *stmt;					// Substatement; non-nil only

		ForStmtNode(uint32 pos, Kind kind, StmtNode *stmt):
				StmtNode(pos, kind), stmt(stmt) {ASSERT(stmt);}
	};
	
	struct ForInStmtNode: StmtNode {
		ExprNode *container;			// Subexpression after 'in'; non-nil only
		StmtNode *stmt;					// Substatement; non-nil only

		ForInStmtNode(uint32 pos, Kind kind, ExprNode *container, StmtNode *stmt):
				StmtNode(pos, kind), container(container), stmt(stmt) {ASSERT(container && stmt);}
	};
	
	struct ForExprInStmtNode: ForInStmtNode {
		ExprNode *varExpr;				// Subexpression before 'in'; non-nil only

		ForExprInStmtNode(uint32 pos, Kind kind, ExprNode *container, StmtNode *stmt, ExprNode *varExpr):
				ForInStmtNode(pos, kind, container, stmt), varExpr(varExpr) {ASSERT(varExpr);}
	};
	
	struct ForVarInStmtNode: ForInStmtNode {
		VariableBinding *binding;		// Var or const binding before 'in'; non-nil only

		ForVarInStmtNode(uint32 pos, Kind kind, ExprNode *container, StmtNode *stmt, VariableBinding *binding):
				ForInStmtNode(pos, kind, container, stmt), binding(binding) {ASSERT(binding);}
	};
	
	struct SwitchStmtNode: ExprStmtNode {
		StmtNode *statements;			// Linked list of switch block's statements, which may include Case and Default statements

		SwitchStmtNode(uint32 pos, Kind kind, ExprNode *expr):
				ExprStmtNode(pos, kind, expr) {}
	};
	
	struct GoStmtNode: StmtNode {
		const StringAtom *label;		// The label; nil if none

		GoStmtNode(uint32 pos, Kind kind, const StringAtom *label): StmtNode(pos, kind), label(label) {}
	};
	
	struct CatchClause: ParseNode {
		CatchClause *next;				// Next catch clause in a linked list of catch clauses
		const StringAtom &name;			// The name of the variable that will hold the exception
		ExprNode *type;					// Type expression or nil if not provided
		StmtNode *stmt;					// The catch clause's body; non-nil only

		CatchClause(uint32 pos, const StringAtom &name, ExprNode *type, StmtNode *stmt):
				ParseNode(pos), next(0), name(name), type(type), stmt(stmt) {ASSERT(stmt);}
	};
	
	struct TryStmtNode: StmtNode {
		StmtNode *stmt;					// Substatement being tried; usually a block; non-nil only
		CatchClause *catches;			// Linked list of catch blocks; may be nil
		StmtNode *finally;				// Finally block or nil if none

		TryStmtNode(uint32 pos, Kind kind, StmtNode *stmt, CatchClause *catches, StmtNode *finally):
				StmtNode(pos, kind), stmt(stmt), catches(catches), finally(finally) {ASSERT(stmt);}
	};

	struct VariableStmtNode: AttributeStmtNode {
		VariableBinding *bindings;		// Linked list of variable bindings

		VariableStmtNode(uint32 pos, Kind kind, ExprList *attributes): AttributeStmtNode(pos, kind, attributes) {}
	};

	struct FunctionStmtNode: AttributeStmtNode {
		FunctionDefinition function;	// Function definition

		FunctionStmtNode(uint32 pos, Kind kind, ExprList *attributes): AttributeStmtNode(pos, kind, attributes) {}
	};

	struct ClassStmtNode: AttributeStmtNode {
		const StringAtom &name;			// The class's name
		ExprList *superclasses;			// Linked list of superclass expressions
		StmtNode *body;					// The class's body; non-nil only

		ClassStmtNode(uint32 pos, Kind kind, ExprList *attributes, const StringAtom &name, ExprList *superclasses,
					  StmtNode *body):
				AttributeStmtNode(pos, kind, attributes), name(name), superclasses(superclasses), body(body) {ASSERT(body);}
	};

	struct LanguageStmtNode: StmtNode {
		JavaScript::Language language;	// The selected language

		LanguageStmtNode(uint32 pos, Kind kind, JavaScript::Language language):
				StmtNode(pos, kind), language(language) {}
	};


	class Parser {
		Lexer lexer;
		Arena &arena;

	  public:
		Parser(World &world, Arena &arena, const String &source, const String &sourceLocation, uint32 initialLineNum = 1);
		
	  private:
		Reader &getReader() {return lexer.reader;}
		World &getWorld() {return lexer.world;}

		void syntaxError(const char *message, uint backUp = 1);
		void syntaxError(const String &message, uint backUp = 1);
		const Token &require(bool preferRegExp, Token::Kind kind);
		String &copyTokenChars(const Token &t);

		IdentifierExprNode *parseQualifiedIdentifier(const Token &t);
		ExprNode *parseIdentifierQualifiers(ExprNode *e, bool &foundQualifiers);
		ExprNode *parseParenthesesAndIdentifierQualifiers(const Token &tParen, bool &foundQualifiers);
		PairListExprNode *parseArrayLiteral(const Token &initialToken);
		PairListExprNode *parseObjectLiteral(const Token &initialToken);
		ExprNode *parsePrimaryExpression();
		ExprNode *parseMember(ExprNode *target, const Token &t, ExprNode::Kind kind, ExprNode::Kind parenKind);
		InvokeExprNode *parseInvoke(ExprNode *target, uint32 pos, Token::Kind closingTokenKind, ExprNode::Kind invokeKind);
		ExprNode *parsePostfixExpression(bool newExpression = false);
	  public:
		ExprNode *parseNonAssignmentExpression(bool noIn);
		ExprNode *parseAssignmentExpression(bool noIn);
		ExprNode *parseExpression(bool noIn);
	};
}
#endif
