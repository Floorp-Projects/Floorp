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

	void escapeString(Formatter &f, const char16 *begin, const char16 *end, char16 quote);
	void quoteString(Formatter &f, const String &s, char16 quote);


	class Token {
	  public:
		enum Kind {	// Keep synchronized with kindNames, kindFlags, and tokenBinaryOperatorInfos tables
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
			Interface,					// interface
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
			Attribute,					// attribute
			Constructor,				// constructor
			Get,						// get
			Language,					// language
			Namespace,					// namespace
			Set,						// set
			Use,						// use
			
			identifier,					// Non-keyword identifier (may be same as a keyword if it contains an escape code)

			kindsEnd,					// End of token kinds
			keywordsBegin = Abstract,	// Beginning of range of special identifier tokens
			keywordsEnd = identifier,	// End of range of special identifier tokens
			nonreservedBegin = Attribute,// Beginning of range of non-reserved words
			nonreservedEnd = identifier,// End of range of non-reserved words
			kindsWithCharsBegin = number,// Beginning of range of tokens for which the chars field (below) is valid
			kindsWithCharsEnd = regExp+1// End of range of tokens for which the chars field (below) is valid
		};

#define CASE_TOKEN_ATTRIBUTE_IDENTIFIER	\
		 Token::Get:					\
	case Token::Set:					\
	case Token::identifier

#define CASE_TOKEN_NONRESERVED			\
		 Token::Attribute:				\
	case Token::Constructor:			\
	case Token::Get:					\
	case Token::Language:				\
	case Token::Namespace:				\
	case Token::Set:					\
	case Token::Use:					\
	case Token::identifier

		enum Flag {
			isAttribute,				// True if this token is an attribute
			canFollowAttribute,			// True if this token is an attribute or can follow an attribute
			canFollowReturn,			// True if this token can follow a return without an expression
			canFollowGet				// True if this token can follow a get or set in a FunctionName
		};

	  private:
		static const char *const kindNames[kindsEnd];
		static const uchar kindFlags[kindsEnd];

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
		static bool isSpecialKind(Kind kind) {return kind <= regExp || kind == identifier;}
		static const char *kindName(Kind kind) {ASSERT(uint(kind) < kindsEnd); return kindNames[kind];}

		Kind getKind() const {ASSERT(valid); return kind;}
		bool hasKind(Kind k) const {ASSERT(valid); return kind == k;}
		bool hasIdentifierKind() const {ASSERT(nonreservedEnd == identifier && kindsEnd == identifier+1); return kind >= nonreservedBegin;}
		bool getFlag(Flag f) const {ASSERT(valid); return (kindFlags[kind] & 1<<f) != 0;}
		
		bool getLineBreak() const {ASSERT(valid); return lineBreak;}
		uint32 getPos() const {ASSERT(valid); return pos;}
		const StringAtom &getIdentifier() const {ASSERT(valid && id); return *id;}
		const String &getChars() const {ASSERT(valid && kind >= kindsWithCharsBegin && kind < kindsWithCharsEnd); return chars;}
		float64 getValue() const {ASSERT(valid && kind == number); return value;}

		friend Formatter &operator<<(Formatter &f, Kind k) {f << kindName(k); return f;}
		void print(Formatter &f, bool debug = false) const;
		
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
	inline void Lexer::redesignate(bool) {}	// See description for the DEBUG version inside parser.cpp
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


	struct ExprNode;
	struct AttributeStmtNode;
	struct BlockStmtNode;
	
	struct FunctionName {
		enum Prefix {
			normal,						// No prefix
			Get,						// get
			Set							// set
		};
		
		Prefix prefix;					// The name's prefix, if any
		ExprNode *name;					// The name; nil if omitted
		
		FunctionName(): prefix(normal), name(0) {}

		void print(PrettyPrinter &f) const;
	};
	
    struct IdentifierList;
    struct VariableBinding: ParseNode {
		VariableBinding *next;			// Next binding in a linked list of variable or parameter bindings
        IdentifierList *aliases;
        ExprNode *name;					// The variable's name; nil if omitted, which currently can only happen for ... parameters
		ExprNode *type;					// Type expression or nil if not provided
		ExprNode *initializer;			// Initial value expression or nil if not provided
		
		VariableBinding(uint32 pos, ExprNode *name, ExprNode *type, ExprNode *initializer=NULL):
				ParseNode(pos), next(0), aliases(0), name(name), type(type), initializer(initializer) {}

		void print(PrettyPrinter &f) const;
	};

	struct FunctionDefinition: FunctionName {
		VariableBinding *parameters;	// Linked list of all parameters, including optional and rest parameters, if any
		VariableBinding *optParameters;	// Pointer to first non-required parameter inside parameters list; nil if none
		VariableBinding *restParameter;	// Pointer to rest parameter inside parameters list; nil if none
		ExprNode *resultType;			// Result type expression or nil if not provided
		BlockStmtNode *body;			// Body; nil if none

		void print(PrettyPrinter &f, bool isConstructor, const AttributeStmtNode *attributes, bool noSemi) const;
	};


	struct ExprNode: ParseNode {
		enum Kind {						// Actual class			Operands	// Keep synchronized with kindNames
			none,
			identifier,					// IdentifierExprNode	<name>		// Begin of isPostfix()
			number,						// NumberExprNode		<value>
			string,						// StringExprNode		<str>
			regExp,						// RegExpExprNode		/<re>/<flags>
			Null,						// ExprNode				null
			True,						// ExprNode				true
			False,						// ExprNode				false
			This,						// ExprNode				this
			Super,						// ExprNode				super

			parentheses,				// UnaryExprNode		(<op>)
			numUnit,					// NumUnitExprNode		<num> "<str>"   or   <num><str>
			exprUnit,					// ExprUnitExprNode		(<op>) "<str>"
			qualify,					// BinaryExprNode		<op1> :: <op2> (right-associative:  a::b::c represented as a::(b::c))

			objectLiteral,				// PairListExprNode		{<field>:<value>, <field>:<value>, ..., <field>:<value>}
			arrayLiteral,				// PairListExprNode		[<value>, <value>, ..., <value>]
			functionLiteral,			// FunctionExprNode		function <function>

			call,						// InvokeExprNode		<op>(<field>:<value>, <field>:<value>, ..., <field>:<value>)
			New,						// InvokeExprNode		new <op>(<field>:<value>, <field>:<value>, ..., <field>:<value>)
			index,						// InvokeExprNode		<op>[<field>:<value>, <field>:<value>, ..., <field>:<value>]

			dot,						// BinaryExprNode		<op1> . <op2>  // <op2> must be identifier or qualify
			dotParen,					// BinaryExprNode		<op1> .( <op2> )
			at,							// BinaryExprNode		<op1> @ <op2>   or   <op1> @( <op2> )
																			// End of isPostfix()

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

			assignment,					// BinaryExprNode		<op1> = <op2>	// Begin of isAssigningKind()
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
			logicalOrEquals,			// BinaryExprNode		<op1> ||= <op2>	// End of isAssigningKind()

			conditional,				// TernaryExprNode		<op1> ? <op2> : <op3>
			comma,						// BinaryExprNode		<op1> , <op2>	// Comma expressions only
			
			kindsEnd
		};
		
	  private:
		Kind kind;						// The node's kind
		static const char *const kindNames[kindsEnd];
	  public:

		ExprNode(uint32 pos, Kind kind): ParseNode(pos), kind(kind) {}

		Kind getKind() const {return kind;}
		bool hasKind(Kind k) const {return kind == k;}

		static bool isFieldKind(Kind kind) {return kind == identifier || kind == number || kind == string || kind == qualify;}
		static bool isAssigningKind(Kind kind) {return kind >= assignment && kind <= logicalOrEquals;}
		static const char *kindName(Kind kind) {ASSERT(uint(kind) < kindsEnd); return kindNames[kind];}
		bool isPostfix() const {return kind >= identifier && kind <= at;}

		virtual void print(PrettyPrinter &f) const;
	};

	// Print e onto f.
	inline PrettyPrinter &operator<<(PrettyPrinter &f, const ExprNode *e) {ASSERT(e); e->print(f); return f;}


	struct IdentifierExprNode: ExprNode {
		const StringAtom &name;			// The identifier

		IdentifierExprNode(uint32 pos, Kind kind, const StringAtom &name): ExprNode(pos, kind), name(name) {}
		explicit IdentifierExprNode(const Token &t): ExprNode(t.getPos(), identifier), name(t.getIdentifier()) {}

		void print(PrettyPrinter &f) const;
	};
	
	struct NumberExprNode: ExprNode {
		float64 value;					// The number's value

		NumberExprNode(uint32 pos, float64 value): ExprNode(pos, number), value(value) {}
		explicit NumberExprNode(const Token &t): ExprNode(t.getPos(), number), value(t.getValue()) {}

		void print(PrettyPrinter &f) const;
	};
	
	struct StringExprNode: ExprNode {
		String &str;					// The string

		StringExprNode(uint32 pos, Kind kind, String &str): ExprNode(pos, kind), str(str) {}

		void print(PrettyPrinter &f) const;
	};
	
	struct RegExpExprNode: ExprNode {
		const StringAtom &re;			// The regular expression's contents
		String &flags;					// The regular expression's flags

		RegExpExprNode(uint32 pos, Kind kind, const StringAtom &re, String &flags):
				ExprNode(pos, kind), re(re), flags(flags) {}

		void print(PrettyPrinter &f) const;
	};
	
	struct NumUnitExprNode: StringExprNode { // str is the unit string
		String &numStr;					// The number's source string
		float64 num;					// The number's value

		NumUnitExprNode(uint32 pos, Kind kind, String &numStr, float64 num, String &unitStr):
				StringExprNode(pos, kind, unitStr), numStr(numStr), num(num) {}

		void print(PrettyPrinter &f) const;
	};
	
	struct ExprUnitExprNode: StringExprNode { // str is the unit string
		ExprNode *op;					// The expression to which the unit is applied; non-nil only

		ExprUnitExprNode(uint32 pos, Kind kind, ExprNode *op, String &unitStr):
				StringExprNode(pos, kind, unitStr), op(op) {ASSERT(op);}

		void print(PrettyPrinter &f) const;
	};

	struct FunctionExprNode: ExprNode {
		FunctionDefinition function;	// Function definition

		explicit FunctionExprNode(uint32 pos): ExprNode(pos, functionLiteral) {}

		void print(PrettyPrinter &f) const;
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

		void print(PrettyPrinter &f) const;
	};

	struct InvokeExprNode: PairListExprNode {
		ExprNode *op;					// The called function, called constructor, or indexed object; non-nil only

		InvokeExprNode(uint32 pos, Kind kind, ExprNode *op, ExprPairList *pairs):
				PairListExprNode(pos, kind, pairs), op(op) {ASSERT(op);}

		void print(PrettyPrinter &f) const;
	};

	struct UnaryExprNode: ExprNode {
		ExprNode *op;					// The unary operator's operand; non-nil only

		UnaryExprNode(uint32 pos, Kind kind, ExprNode *op): ExprNode(pos, kind), op(op) {ASSERT(op);}

		void print(PrettyPrinter &f) const;
	};

	struct BinaryExprNode: ExprNode {
		ExprNode *op1;					// The binary operator's first operand; non-nil only
		ExprNode *op2;					// The binary operator's second operand; non-nil only

		BinaryExprNode(uint32 pos, Kind kind, ExprNode *op1, ExprNode *op2):
				ExprNode(pos, kind), op1(op1), op2(op2) {ASSERT(op1 && op2);}

		void print(PrettyPrinter &f) const;
	};

	struct TernaryExprNode: ExprNode {
		ExprNode *op1;					// The ternary operator's first operand; non-nil only
		ExprNode *op2;					// The ternary operator's second operand; non-nil only
		ExprNode *op3;					// The ternary operator's third operand; non-nil only

		TernaryExprNode(uint32 pos, Kind kind, ExprNode *op1, ExprNode *op2, ExprNode *op3):
				ExprNode(pos, kind), op1(op1), op2(op2), op3(op3) {ASSERT(op1 && op2 && op3);}

		void print(PrettyPrinter &f) const;
	};



	struct StmtNode: ParseNode {
		enum Kind {						// Actual class			Operands
			empty,						// StmtNode				;
			expression,					// ExprStmtNode			<expr> ;
			block,						// BlockStmtNode		<attributes> { <statements> }
			label,						// LabelStmtNode		<name> : <stmt>
			If,							// UnaryStmtNode		if ( <expr> ) <stmt>
			IfElse,						// BinaryStmtNode		if ( <expr> ) <stmt> else <stmt2>
			Switch,						// SwitchStmtNode		switch ( <expr> ) <statements>
			While,						// UnaryStmtNode		while ( <expr> ) <stmt>
			DoWhile,					// UnaryStmtNode		do <stmt> while ( <expr> )
			With,						// UnaryStmtNode		with ( <expr> ) <stmt>
			For,						// ForStmtNode			for ( <initializer> ; <expr2> ; <expr3> ) <stmt>
			ForIn,						// ForStmtNode			for ( <initializer> in <expr2> ) <stmt>
			Case,						// ExprStmtNode			case <expr> :	or   default :	// Only occurs directly inside a Switch
			Break,						// GoStmtNode			break ;   or   break <name> ;
			Continue,					// GoStmtNode			continue ;   or   continue <name> ;
			Return,						// ExprStmtNode			return ;   or   return <expr> ;
			Throw,						// ExprStmtNode			throw <expr> ;
			Try,						// TryStmtNode			try <stmt> <catches> <finally>
			Import,						// ImportStmtNode		import <bindings> ;
			UseImport,					// ImportStmtNode		use import <bindings> ;
			Use,						// UseStmtNode			use namespace <exprs> ;
			Export,						// ExportStmtNode		<attributes> export <bindings> ;
			Const,						// VariableStmtNode		<attributes> const <bindings> ;
			Var,						// VariableStmtNode		<attributes> var <bindings> ;
			Function,					// FunctionStmtNode		<attributes> function <function>
			Constructor,				// FunctionStmtNode		<attributes> constructor <function>
			Class,						// ClassStmtNode		<attributes> class <name> extends <superclass> implements <supers> <body>
			Interface,					// ClassStmtNode		<attributes> interface <name> extends <supers> <body>
			Namespace,					// NamespaceStmtNode	<attributes> namespace <name> extends <supers>
			Language,					// LanguageStmtNode		language <language> ;
			Package,    				// PackageStmtNode		package <packageName> <body>
            Debugger                    // ExprStmtNode         debugger ;
		};
		
	  private:
		Kind kind;						// The node's kind
	  public:
		StmtNode *next;					// Next statement in a linked list of statements in this block

		StmtNode(uint32 pos, Kind kind): ParseNode(pos), kind(kind), next(0) {}

		Kind getKind() const {return kind;}
		bool hasKind(Kind k) const {return kind == k;}

		static void printStatements(PrettyPrinter &f, const StmtNode *statements);
		static void printBlockStatements(PrettyPrinter &f, const StmtNode *statements, bool loose);
		static void printSemi(PrettyPrinter &f, bool noSemi);
		void printSubstatement(PrettyPrinter &f, bool noSemi, const char *continuation = 0) const;
		virtual void print(PrettyPrinter &f, bool noSemi) const;
	};


	struct ExprStmtNode: StmtNode {
		ExprNode *expr;					// The expression statement's expression.  May be nil for default: or return-with-no-expression statements.

		ExprStmtNode(uint32 pos, Kind kind, ExprNode *expr): StmtNode(pos, kind), expr(expr) {}

		void print(PrettyPrinter &f, bool noSemi) const;
	};

	struct DebuggerStmtNode: StmtNode {
		DebuggerStmtNode(uint32 pos, Kind kind): StmtNode(pos, kind) {}

		void print(PrettyPrinter &f, bool noSemi) const;
	};

	struct IdentifierList: ArenaObject {
		IdentifierList *next;			// Next identifier in linked list
		const StringAtom &name;			// The identifier
		
		explicit IdentifierList(const StringAtom &name): next(0), name(name) {}
	};
	
	struct AttributeStmtNode: StmtNode {
		IdentifierList *attributes;		// Linked list of block or definition's attributes

		AttributeStmtNode(uint32 pos, Kind kind, IdentifierList *attributes): StmtNode(pos, kind), attributes(attributes) {}

		void printAttributes(PrettyPrinter &f) const;
	};
	
	struct BlockStmtNode: AttributeStmtNode {
		StmtNode *statements;			// Linked list of block's statements

		BlockStmtNode(uint32 pos, Kind kind, IdentifierList *attributes, StmtNode *statements):
				AttributeStmtNode(pos, kind, attributes), statements(statements) {}

		void print(PrettyPrinter &f, bool noSemi) const;
		void printBlock(PrettyPrinter &f, bool loose) const;
	};
	
	struct LabelStmtNode: StmtNode {
		const StringAtom &name;			// The label
		StmtNode *stmt;					// Labeled statement; non-nil only

		LabelStmtNode(uint32 pos, const StringAtom &name, StmtNode *stmt):
				StmtNode(pos, label), name(name), stmt(stmt) {ASSERT(stmt);}

		void print(PrettyPrinter &f, bool noSemi) const;
	};
	
	struct UnaryStmtNode: ExprStmtNode {
		StmtNode *stmt;					// First substatement; non-nil only

		UnaryStmtNode(uint32 pos, Kind kind, ExprNode *expr, StmtNode *stmt):
				ExprStmtNode(pos, kind, expr), stmt(stmt) {ASSERT(stmt);}

		void print(PrettyPrinter &f, bool noSemi) const;
		virtual void printContents(PrettyPrinter &f, bool noSemi) const;
	};
	
	struct BinaryStmtNode: UnaryStmtNode {
		StmtNode *stmt2;				// Second substatement; non-nil only

		BinaryStmtNode(uint32 pos, Kind kind, ExprNode *expr, StmtNode *stmt1, StmtNode *stmt2):
				UnaryStmtNode(pos, kind, expr, stmt1), stmt2(stmt2) {ASSERT(stmt2);}

		void printContents(PrettyPrinter &f, bool noSemi) const;
	};
	
	struct ForStmtNode: StmtNode {
		StmtNode *initializer;			// For: First item in parentheses; either nil (if not provided), an expression, or a Var, or a Const.
										// ForIn: Expression or declaration before 'in'; either an expression, or a Var or a Const with exactly one binding.
		ExprNode *expr2;				// For: Second item in parentheses; nil if not provided
										// ForIn: Subexpression after 'in'; non-nil only
		ExprNode *expr3;				// For: Third item in parentheses; nil if not provided
										// ForIn: nil
		StmtNode *stmt;					// Substatement; non-nil only

		ForStmtNode(uint32 pos, Kind kind, StmtNode *initializer, ExprNode *expr2, ExprNode *expr3, StmtNode *stmt):
				StmtNode(pos, kind), initializer(initializer), expr2(expr2), expr3(expr3), stmt(stmt) {ASSERT(stmt);}

		void print(PrettyPrinter &f, bool noSemi) const;
	};
	
	struct SwitchStmtNode: ExprStmtNode {
		StmtNode *statements;			// Linked list of switch block's statements, which may include Case and Default statements

		SwitchStmtNode(uint32 pos, ExprNode *expr, StmtNode *statements): ExprStmtNode(pos, Switch, expr), statements(statements) {}

		void print(PrettyPrinter &f, bool noSemi) const;
	};
	
	struct GoStmtNode: StmtNode {
		const StringAtom *name;			// The label; nil if none

		GoStmtNode(uint32 pos, Kind kind, const StringAtom *name): StmtNode(pos, kind), name(name) {}

		void print(PrettyPrinter &f, bool noSemi) const;
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

		TryStmtNode(uint32 pos, StmtNode *stmt, CatchClause *catches, StmtNode *finally):
				StmtNode(pos, Try), stmt(stmt), catches(catches), finally(finally) {ASSERT(stmt);}

		void print(PrettyPrinter &f, bool noSemi) const;
	};

	struct PackageName: ArenaObject {	// Either idList or str may be null, but not both
		IdentifierList *idList;			// The package name as an identifier list
		String *str;					// The package name as a string
		
		explicit PackageName(IdentifierList *idList): idList(idList), str(0) {ASSERT(idList);}
		explicit PackageName(String &str): idList(0), str(&str) {}
	};
	
	struct ImportBinding: ParseNode {
		ImportBinding *next;			// Next binding in a linked list of import bindings
		const StringAtom *name;			// The package variable's name; nil if omitted
		PackageName &packageName;		// The package's name
		
		ImportBinding(uint32 pos, const StringAtom *name, PackageName &packageName):
				ParseNode(pos), next(0), name(name), packageName(packageName) {}
	};

	struct ImportStmtNode: StmtNode {
		ImportBinding *bindings;		// Linked list of import bindings

		ImportStmtNode(uint32 pos, Kind kind, ImportBinding *bindings): StmtNode(pos, kind), bindings(bindings) {}
	};

	struct ExprList: ArenaObject {
		ExprList *next;					// Next expression in linked list
		ExprNode *expr;					// Attribute expression; non-nil only
		
		explicit ExprList(ExprNode *expr): next(0), expr(expr) {ASSERT(expr);}
		
		void printCommaList(PrettyPrinter &f) const;
		static void printOptionalCommaList(PrettyPrinter &f, const char *name, const ExprList *list);
	};
	
	struct UseStmtNode: StmtNode {
		ExprList *exprs;				// Linked list of namespace expressions

		UseStmtNode(uint32 pos, Kind kind, ExprList *exprs): StmtNode(pos, kind), exprs(exprs) {}
	};
	
	struct ExportBinding: ParseNode {
		ExportBinding *next;			// Next binding in a linked list of export bindings
		FunctionName name;				// The exported variable's name
		FunctionName *initializer;		// The original variable's name or nil if not provided
		
		ExportBinding(uint32 pos, FunctionName *initializer): ParseNode(pos), next(0), initializer(initializer) {}
	};
	
	struct ExportStmtNode: AttributeStmtNode {
		ExportBinding *bindings;		// Linked list of export bindings

		ExportStmtNode(uint32 pos, Kind kind, IdentifierList *attributes, ExportBinding *bindings):
				AttributeStmtNode(pos, kind, attributes), bindings(bindings) {}
	};

	struct VariableStmtNode: AttributeStmtNode {
		VariableBinding *bindings;		// Linked list of variable bindings

		VariableStmtNode(uint32 pos, Kind kind, IdentifierList *attributes, VariableBinding *bindings):
				AttributeStmtNode(pos, kind, attributes), bindings(bindings) {}

		void print(PrettyPrinter &f, bool noSemi) const;
	};

	struct FunctionStmtNode: AttributeStmtNode {
		FunctionDefinition function;	// Function definition

		FunctionStmtNode(uint32 pos, Kind kind, IdentifierList *attributes): AttributeStmtNode(pos, kind, attributes) {}

		void print(PrettyPrinter &f, bool noSemi) const;
	};

	struct NamespaceStmtNode: AttributeStmtNode {
		ExprNode *name;					// The namespace's, interfaces, or class's name; non-nil only
		ExprList *supers;				// Linked list of supernamespace or superinterface expressions

		NamespaceStmtNode(uint32 pos, Kind kind, IdentifierList *attributes, ExprNode *name, ExprList *supers):
			AttributeStmtNode(pos, kind, attributes), name(name), supers(supers) {ASSERT(name);}

		void print(PrettyPrinter &f, bool noSemi) const;
	};

	struct ClassStmtNode: NamespaceStmtNode {
		ExprNode *superclass;			// Superclass expression (classes only); nil if omitted
		BlockStmtNode *body;			// The class's body; nil if omitted

		ClassStmtNode(uint32 pos, Kind kind, IdentifierList *attributes, ExprNode *name, ExprNode *superclass, ExprList *superinterfaces,
					  BlockStmtNode *body):
			NamespaceStmtNode(pos, kind, attributes, name, superinterfaces), superclass(superclass), body(body) {}

		void print(PrettyPrinter &f, bool noSemi) const;
	};

	struct LanguageStmtNode: StmtNode {
		JavaScript::Language language;	// The selected language

		LanguageStmtNode(uint32 pos, Kind kind, JavaScript::Language language):
				StmtNode(pos, kind), language(language) {}
	};

	struct PackageStmtNode: StmtNode {
		PackageName &packageName;		// The package's name
		BlockStmtNode *body;			// The package's body; non-nil only

		PackageStmtNode(uint32 pos, Kind kind, PackageName &packageName, BlockStmtNode *body):
				StmtNode(pos, kind), packageName(packageName), body(body) {ASSERT(body);}
	};


	class Parser {
	  public:
		Lexer lexer;
		Arena &arena;
		bool lineBreaksSignificant;		// If false, line breaks between tokens are treated as though they were spaces instead

		Parser(World &world, Arena &arena, const String &source, const String &sourceLocation, uint32 initialLineNum = 1);
		
	  private:
		Reader &getReader() {return lexer.reader;}
		World &getWorld() {return lexer.world;}

	  public:
		void syntaxError(const char *message, uint backUp = 1);
		void syntaxError(const String &message, uint backUp = 1);
		const Token &require(bool preferRegExp, Token::Kind kind);
	  private:
		String &copyTokenChars(const Token &t);
		bool lineBreakBefore(const Token &t) const {return lineBreaksSignificant && t.getLineBreak();}
		bool lineBreakBefore() {return lineBreaksSignificant && lexer.peek(true).getLineBreak();}

		ExprNode *makeIdentifierExpression(const Token &t) const;
		ExprNode *parseIdentifierQualifiers(ExprNode *e, bool &foundQualifiers, bool preferRegExp);
		ExprNode *parseParenthesesAndIdentifierQualifiers(const Token &tParen, bool &foundQualifiers, bool preferRegExp);
		ExprNode *parseQualifiedIdentifier(const Token &t, bool preferRegExp);
		PairListExprNode *parseArrayLiteral(const Token &initialToken);
		PairListExprNode *parseObjectLiteral(const Token &initialToken);
		ExprNode *parsePrimaryExpression();
		BinaryExprNode *parseMember(ExprNode *target, const Token &tOperator, ExprNode::Kind kind, ExprNode::Kind parenKind);
		InvokeExprNode *parseInvoke(ExprNode *target, uint32 pos, Token::Kind closingTokenKind, ExprNode::Kind invokeKind);
		ExprNode *parsePostfixExpression(bool newExpression = false);
		void ensurePostfix(const ExprNode *e);
		ExprNode *parseUnaryExpression();

		enum Precedence {
			pNone,				// End tag
			pExpression,		// Expression
			pAssignment,		// AssignmentExpression
			pConditional,		// ConditionalExpression
			pLogicalOr,			// LogicalOrExpression
			pLogicalXor,		// LogicalXorExpression
			pLogicalAnd,		// LogicalAndExpression
			pBitwiseOr,			// BitwiseOrExpression
			pBitwiseXor,		// BitwiseXorExpression
			pBitwiseAnd,		// BitwiseAndExpression
			pEquality,			// EqualityExpression
			pRelational,		// RelationalExpression
			pShift,				// ShiftExpression
			pAdditive,			// AdditiveExpression
			pMultiplicative,	// MultiplicativeExpression
			pUnary,				// UnaryExpression
			pPostfix			// PostfixExpression
		};

		struct BinaryOperatorInfo {
			ExprNode::Kind kind;		// The kind of BinaryExprNode the operator should generate; ExprNode::none if not a binary operator
			Precedence precedenceLeft;	// Operators in this operator's left subexpression with precedenceLeft or higher are reduced
			Precedence precedenceRight;	// This operator's precedence
		};
		static const BinaryOperatorInfo tokenBinaryOperatorInfos[Token::kindsEnd];
		struct StackedSubexpression;

	  public:
		ExprNode *parseExpression(bool noIn, bool noAssignment = false, bool noComma = false);
		ExprNode *parseNonAssignmentExpression(bool noIn) {return parseExpression(noIn, true, true);}
		ExprNode *parseAssignmentExpression(bool noIn=false) {return parseExpression(noIn, false, true);}
	  private:
		ExprNode *parseParenthesizedExpression();
		ExprNode *parseTypeExpression(bool noIn=false);
		const StringAtom &parseTypedIdentifier(ExprNode *&type);
		ExprNode *parseTypeBinding(Token::Kind kind, bool noIn);
		ExprList *parseTypeListBinding(Token::Kind kind);
		VariableBinding *parseVariableBinding(bool noQualifiers, bool noIn);
		void parseFunctionName(FunctionName &fn);
		void parseFunctionSignature(FunctionDefinition &fd);
		
		enum SemicolonState {semiNone, semiNoninsertable, semiInsertable};
		enum AttributeStatement {asAny, asBlock, asConstVar};
		StmtNode *parseBlock(bool inSwitch, bool noCloseBrace);
		BlockStmtNode *parseBody(SemicolonState *semicolonState);
		StmtNode *parseAttributeStatement(uint32 pos, IdentifierList *attributes, const Token &t, bool noIn, SemicolonState &semicolonState);
		StmtNode *parseAttributesAndStatement(const Token *t, AttributeStatement as, SemicolonState &semicolonState);
		StmtNode *parseAnnotatedBlock();
		StmtNode *parseFor(uint32 pos, SemicolonState &semicolonState);
		StmtNode *parseTry(uint32 pos);
	  public:
		StmtNode *parseStatement(bool topLevel, bool inSwitch, SemicolonState &semicolonState);
		StmtNode *parseStatementAndSemicolon(SemicolonState &semicolonState);
		StmtNode *parseProgram() {return parseBlock(false, true);}
      private:        
        bool lookahead(Token::Kind  kind,bool preferRegExp=true);
        const Token *match(Token::Kind  kind,bool preferRegExp=true);
        ExprNode  *parseIdentifier();
        ExprPairList *parseLiteralField();
        ExprNode *parseFieldName();
        ExprPairList *parseArgumentList(NodeQueue<ExprPairList> &args);
        ExprPairList *parseArgumentListPrime(NodeQueue<ExprPairList> &$$);
        ExprPairList *parseNamedArgumentListPrime(NodeQueue<ExprPairList> &$$);
        VariableBinding *parseAllParameters(FunctionDefinition &fd,NodeQueue<VariableBinding> &params);
        VariableBinding *parseNamedParameters(FunctionDefinition &fd,NodeQueue<VariableBinding> &params);
        VariableBinding *parseRestParameter();
        VariableBinding *parseParameter();
        VariableBinding *parseOptionalNamedRestParameters(FunctionDefinition &fd,NodeQueue<VariableBinding> &params);
        VariableBinding *parseNamedRestParameters(FunctionDefinition &fd,NodeQueue<VariableBinding> &params);
        VariableBinding *parseOptionalParameter();
        VariableBinding *parseOptionalParameterPrime(VariableBinding* $1);
        VariableBinding *parseNamedParameter(NodeQueue<IdentifierList> &aliases);
        ExprNode  *parseResultSignature();
	};
}
#endif
