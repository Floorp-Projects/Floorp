/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#ifndef parser_h___
#define parser_h___

#include "world.h"
#include "utilities.h"
#include "reader.h"
#include "lexer.h"
#include <vector>

#include "property.h"

namespace JavaScript {

    // forward declarations to classes in the back-end
    namespace JS2Runtime {
        class JSFunction;
        class JSType;
        class JSObject;
        class Attribute;
    }


//
// Pragmas
//

    namespace Pragma {
        enum Flags {                    // Bitmap of pragma flags
            // Only one of js1, js12, es4, or js2 will be set
            js1 = 1<<0,                 // JS 1.x and ECMAScript 1, 2, or 3
            js12 = 1<<1,                // JS 1.2
            es4 = 1<<2,                 // ECMAScript 4
            js2 = 1<<3,                 // ECMAScript 4 with JS 2.0 extensions

            js1or12 = js1|js12,
            es4orjs2 = es4|js2,
            languageMask = js1|js12|es4|js2,

            strict = 1<<4               // Strict mode; can only be set when js2 or es4 is set
        };
        
        inline Flags setLanguage(Flags flags, Flags language) {return static_cast<Flags>(flags & ~languageMask | language);}
        inline bool lineBreaksSignificant(Flags flags) {return (flags & strict) == 0;}
    }


//
// Parser
//

    // The structures below are generally allocated inside an arena.  The
    // structures' destructors may never be called, so these structures should not
    // hold onto any data that needs to be destroyed explicitly.  Strings are
    // allocated via newArenaString.
    struct ParseNode: ArenaObject {
        size_t pos;                     // Position of this statement or expression

        explicit ParseNode(size_t pos): pos(pos) {}
    };


    // A helper template for creating linked lists of ParseNode subtypes.  N should
    // be derived from a ParseNode and should have an instance variable called
    // <next> of type N* and that is initialized to nil when an N instance is
    // created.
    template<class N> class NodeQueue {
      public:
        N *first;                       // Head of queue
      private:
        N **last;                       // Next link of last element of queue

      public:
        NodeQueue(): first(0), last(&first) {}
      private:
        NodeQueue(const NodeQueue&);        // No copy constructor
        void operator=(const NodeQueue&);   // No assignment operator
      public:
        void operator+=(N *elt) {ASSERT(elt && !elt->next); *last = elt; last = &elt->next;}
    };


    struct ExprNode;
    struct AttributeStmtNode;
    struct BlockStmtNode;


    struct VariableBinding: ParseNode {
        VariableBinding *next;          // Next binding in a linked list of variable or parameter bindings
        const StringAtom *name;         // The variable's name;
                                        // nil if omitted, which currently can only happen for ... parameters
        ExprNode *type;                 // Type expression or nil if not provided
        ExprNode *initializer;          // Initial value expression or nil if not provided
        bool constant;                  // true for const variables and parameters

        JS2Runtime::Property *prop;     // the sematics/codegen passes stuff their data in here.
        JS2Runtime::JSObject *scope;    // ditto

        VariableBinding(size_t pos, const StringAtom *name, ExprNode *type, ExprNode *initializer, bool constant):
                ParseNode(pos), next(0), name(name), type(type), initializer(initializer), constant(constant) {}

        void print(PrettyPrinter &f, bool printConst) const;
    };


    struct ExprNode: ParseNode {
        // Keep synchronized with kindNames
        enum Kind {                     // Actual class         Operands
            none,

            // Beginning of isPostfix()
            identifier,                 // IdentifierExprNode   <name>
            Null,                       // ExprNode             null
            boolean,                    // BooleanExprNode      <value>
            number,                     // NumberExprNode       <value>
            string,                     // StringExprNode       <str>
            regExp,                     // RegExpExprNode       /<re>/<flags>
            This,                       // ExprNode             this

            parentheses,                // UnaryExprNode        (<op>)
            numUnit,                    // NumUnitExprNode      <num> "<str>" or <num><str>
            exprUnit,                   // ExprUnitExprNode     (<op>) "<str>"  or  <op><str>
            qualify,                    // QualifyExprNode      <qualifier> :: <name>

            objectLiteral,              // PairListExprNode     {<field>:<value>, <field>:<value>, ..., <field>:<value>}
            arrayLiteral,               // PairListExprNode     [<value>, <value>, ..., <value>]
            functionLiteral,            // FunctionExprNode     function <function>

            call,                       // InvokeExprNode       <op>(<field>:<value>, <field>:<value>, ..., <field>:<value>)
            New,                        // InvokeExprNode       new <op>(<field>:<value>, <field>:<value>, ..., <field>:<value>)
            index,                      // InvokeExprNode       <op>[<field>:<value>, <field>:<value>, ..., <field>:<value>]

            dot,                        // BinaryExprNode       <op1> . <op2>   (<op2> must be identifier or qualify)
            dotClass,                   // UnaryExprNode        <op1> .class
            dotParen,                   // BinaryExprNode       <op1> .( <op2> )
            // End of isPostfix()

            superExpr,                  // SuperExprNode        super or super(<op>)
            superStmt,                  // InvokeExprNode       super(<field>:<value>, <field>:<value>, ..., <field>:<value>)
                                        // A superStmt will only appear at the top level of an expression StmtNode.

            Delete,                     // UnaryExprNode        delete <op>
            Void,                       // UnaryExprNode        void <op>
            Typeof,                     // UnaryExprNode        typeof <op>
            preIncrement,               // UnaryExprNode        ++ <op>
            preDecrement,               // UnaryExprNode        -- <op>
            postIncrement,              // UnaryExprNode        <op> ++
            postDecrement,              // UnaryExprNode        <op> --
            plus,                       // UnaryExprNode        + <op>
            minus,                      // UnaryExprNode        - <op>
            complement,                 // UnaryExprNode        ~ <op>
            logicalNot,                 // UnaryExprNode        ! <op>

            juxtapose,                  // BinaryExprNode       <op1> <op2>   (used to combine attributes)
            add,                        // BinaryExprNode       <op1> + <op2>
            subtract,                   // BinaryExprNode       <op1> - <op2>
            multiply,                   // BinaryExprNode       <op1> * <op2>
            divide,                     // BinaryExprNode       <op1> / <op2>
            modulo,                     // BinaryExprNode       <op1> % <op2>
            leftShift,                  // BinaryExprNode       <op1> << <op2>
            rightShift,                 // BinaryExprNode       <op1> >> <op2>
            logicalRightShift,          // BinaryExprNode       <op1> >>> <op2>
            bitwiseAnd,                 // BinaryExprNode       <op1> & <op2>
            bitwiseXor,                 // BinaryExprNode       <op1> ^ <op2>
            bitwiseOr,                  // BinaryExprNode       <op1> | <op2>
            logicalAnd,                 // BinaryExprNode       <op1> && <op2>
            logicalXor,                 // BinaryExprNode       <op1> ^^ <op2>
            logicalOr,                  // BinaryExprNode       <op1> || <op2>

            equal,                      // BinaryExprNode       <op1> == <op2>
            notEqual,                   // BinaryExprNode       <op1> != <op2>
            lessThan,                   // BinaryExprNode       <op1> < <op2>
            lessThanOrEqual,            // BinaryExprNode       <op1> <= <op2>
            greaterThan,                // BinaryExprNode       <op1> > <op2>
            greaterThanOrEqual,         // BinaryExprNode       <op1> >= <op2>
            identical,                  // BinaryExprNode       <op1> === <op2>
            notIdentical,               // BinaryExprNode       <op1> !== <op2>
            As,                         // BinaryExprNode       <op1> as <op2>
            In,                         // BinaryExprNode       <op1> in <op2>
            Instanceof,                 // BinaryExprNode       <op1> instanceof <op2>
            Is,                         // BinaryExprNode       <op1> is <op2>

            assignment,                 // BinaryExprNode       <op1> = <op2>
            addEquals,                  // BinaryExprNode       <op1> += <op2>
            subtractEquals,             // BinaryExprNode       <op1> -= <op2>
            multiplyEquals,             // BinaryExprNode       <op1> *= <op2>
            divideEquals,               // BinaryExprNode       <op1> /= <op2>
            moduloEquals,               // BinaryExprNode       <op1> %= <op2>
            leftShiftEquals,            // BinaryExprNode       <op1> <<= <op2>
            rightShiftEquals,           // BinaryExprNode       <op1> >>= <op2>
            logicalRightShiftEquals,    // BinaryExprNode       <op1> >>>= <op2>
            bitwiseAndEquals,           // BinaryExprNode       <op1> &= <op2>
            bitwiseXorEquals,           // BinaryExprNode       <op1> ^= <op2>
            bitwiseOrEquals,            // BinaryExprNode       <op1> |= <op2>
            logicalAndEquals,           // BinaryExprNode       <op1> &&= <op2>
            logicalXorEquals,           // BinaryExprNode       <op1> ^^= <op2>
            logicalOrEquals,            // BinaryExprNode       <op1> ||= <op2>

            conditional,                // TernaryExprNode      <op1> ? <op2> : <op3>
            comma,                      // BinaryExprNode       <op1> , <op2>  (Comma expressions only)

            kindsEnd
        };

      private:
        Kind kind;                      // The node's kind
        static const char *const kindNames[kindsEnd];
      public:

        ExprNode(size_t pos, Kind kind): ParseNode(pos), kind(kind) {}

        Kind getKind() const {return kind;}
        bool hasKind(Kind k) const {return kind == k;}

        static const char *kindName(Kind kind) {ASSERT(uint(kind) < kindsEnd); return kindNames[kind];}
        bool isPostfix() const {return kind >= identifier && kind <= dotParen;}

        virtual void print(PrettyPrinter &f) const;
        friend Formatter &operator<<(Formatter &f, Kind k) {f << kindName(k); return f;}
    };

    // Print e onto f.
    inline PrettyPrinter &operator<<(PrettyPrinter &f, const ExprNode *e) {
        ASSERT(e); e->print(f); return f;
    }


    struct FunctionName {
        enum Prefix {                   // Keep synchronized with functionPrefixNames
            normal,                     // No prefix
            Get,                        // get
            Set,                        // set
            op                          // The function name is a string (used for operator overrides)
        };

        Prefix prefix;                  // The name's prefix, if any
        const StringAtom *name;         // The interned name; nil if omitted

        FunctionName(): prefix(normal), name(0) {}

        void print(PrettyPrinter &f) const;
    };


    struct FunctionDefinition: FunctionName {
        VariableBinding *parameters;    // Linked list of all parameters, including optional and rest parameters, if any
        VariableBinding *optParameters; // Pointer to first non-required (or rest) parameter inside parameters list; nil if none
        VariableBinding *restParameter; // Pointer to rest parameter inside parameters list; nil if none
        ExprNode *resultType;           // Result type expression or nil if not provided
        BlockStmtNode *body;            // Body; nil if none

        void print(PrettyPrinter &f, const AttributeStmtNode *attributes, bool noSemi) const;
    };


    struct IdentifierExprNode: ExprNode {
        const StringAtom &name;         // The identifier

        IdentifierExprNode(size_t pos, Kind kind, const StringAtom &name): ExprNode(pos, kind), name(name) {}
        explicit IdentifierExprNode(const Token &t): ExprNode(t.getPos(), identifier), name(t.getIdentifier()) {}

        void print(PrettyPrinter &f) const;
    };

    struct QualifyExprNode: IdentifierExprNode {
        ExprNode *qualifier;            // The qualifier expression; non-nil only

        QualifyExprNode(size_t pos, ExprNode *qualifier, const StringAtom &name):
                IdentifierExprNode(pos, qualify, name), qualifier(qualifier) {ASSERT(qualifier);}

        void print(PrettyPrinter &f) const;
    };

    struct BooleanExprNode: ExprNode {
        bool value;                     // The boolean's value

        BooleanExprNode(size_t pos, bool value): ExprNode(pos, boolean), value(value) {}

        void print(PrettyPrinter &f) const;
    };

    struct NumberExprNode: ExprNode {
        float64 value;                  // The number's value

        NumberExprNode(size_t pos, float64 value): ExprNode(pos, number), value(value) {}
        explicit NumberExprNode(const Token &t): ExprNode(t.getPos(), number), value(t.getValue()) {}

        void print(PrettyPrinter &f) const;
    };

    struct StringExprNode: ExprNode {
        String &str;                    // The string

        StringExprNode(size_t pos, Kind kind, String &str): ExprNode(pos, kind), str(str) {}

        void print(PrettyPrinter &f) const;
    };

    struct RegExpExprNode: ExprNode {
        const StringAtom &re;           // The regular expression's contents
        String &flags;                  // The regular expression's flags

        RegExpExprNode(size_t pos, Kind kind, const StringAtom &re, String &flags):
                ExprNode(pos, kind), re(re), flags(flags) {}

        void print(PrettyPrinter &f) const;
    };

    struct NumUnitExprNode: StringExprNode { // str is the unit string
        String &numStr;                 // The number's source string
        float64 num;                    // The number's value

        NumUnitExprNode(size_t pos, Kind kind, String &numStr, float64 num, String &unitStr):
                StringExprNode(pos, kind, unitStr), numStr(numStr), num(num) {}

        void print(PrettyPrinter &f) const;
    };

    struct ExprUnitExprNode: StringExprNode { // str is the unit string
        ExprNode *op;                         // The expression to which the unit is applied; non-nil only

        ExprUnitExprNode(size_t pos, Kind kind, ExprNode *op, String &unitStr):
                StringExprNode(pos, kind, unitStr), op(op) {ASSERT(op);}

        void print(PrettyPrinter &f) const;
    };

    struct FunctionExprNode: ExprNode {
        FunctionDefinition function;    // Function definition

        explicit FunctionExprNode(size_t pos): ExprNode(pos, functionLiteral) {}

        void print(PrettyPrinter &f) const;
    };

    struct ExprPairList: ArenaObject {
        ExprPairList *next;             // Next pair in linked list
        ExprNode *field;                // Field expression or nil if not provided
        ExprNode *value;                // Value expression or nil if not provided

        explicit ExprPairList(ExprNode *field = 0, ExprNode *value = 0): next(0), field(field), value(value) {}
    };

    struct PairListExprNode: ExprNode {
        ExprPairList *pairs;            // Linked list of pairs

        PairListExprNode(size_t pos, Kind kind, ExprPairList *pairs): ExprNode(pos, kind), pairs(pairs) {}

        void print(PrettyPrinter &f) const;
    };

    struct InvokeExprNode: PairListExprNode {
        ExprNode *op;                   // The called function, called constructor, or indexed object; nil only for superStmt
        bool isSuperInvoke;             // used by backend to handle super constructor call in a constructor

        InvokeExprNode(size_t pos, Kind kind, ExprNode *op, ExprPairList *pairs):
                PairListExprNode(pos, kind, pairs), op(op), isSuperInvoke(false) {ASSERT(op || kind == superStmt);}

        void print(PrettyPrinter &f) const;
    };

    struct SuperExprNode: ExprNode {
        ExprNode *op;                   // The operand or nil if none

        SuperExprNode(size_t pos, ExprNode *op): ExprNode(pos, superExpr), op(op) {}

        void print(PrettyPrinter &f) const;
    };

    struct UnaryExprNode: ExprNode {
        ExprNode *op;                   // The unary operator's operand; non-nil only

        UnaryExprNode(size_t pos, Kind kind, ExprNode *op): ExprNode(pos, kind), op(op) {ASSERT(op);}

        void print(PrettyPrinter &f) const;
    };

    struct BinaryExprNode: ExprNode {
        ExprNode *op1;                  // The binary operator's first operand; non-nil only
        ExprNode *op2;                  // The binary operator's second operand; non-nil only

        BinaryExprNode(size_t pos, Kind kind, ExprNode *op1, ExprNode *op2):
                ExprNode(pos, kind), op1(op1), op2(op2) {ASSERT(op1 && op2);}

        void print(PrettyPrinter &f) const;
    };

    struct TernaryExprNode: ExprNode {
        ExprNode *op1;                  // The ternary operator's first operand; non-nil only
        ExprNode *op2;                  // The ternary operator's second operand; non-nil only
        ExprNode *op3;                  // The ternary operator's third operand; non-nil only

        TernaryExprNode(size_t pos, Kind kind, ExprNode *op1, ExprNode *op2, ExprNode *op3):
                ExprNode(pos, kind), op1(op1), op2(op2), op3(op3) {ASSERT(op1 && op2 && op3);}

        void print(PrettyPrinter &f) const;
    };


    struct StmtNode: ParseNode {
        enum Kind {         // Actual class         Operands
            empty,          // StmtNode             ;
            expression,     // ExprStmtNode         <expr> ;
            block,          // BlockStmtNode        <attributes> { <statements> }
            label,          // LabelStmtNode        <name> : <stmt>
            If,             // UnaryStmtNode        if ( <expr> ) <stmt>
            IfElse,         // BinaryStmtNode       if ( <expr> ) <stmt> else <stmt2>
            Switch,         // SwitchStmtNode       switch ( <expr> ) <statements>
            While,          // UnaryStmtNode        while ( <expr> ) <stmt>
            DoWhile,        // UnaryStmtNode        do <stmt> while ( <expr> )
            With,           // UnaryStmtNode        with ( <expr> ) <stmt>
            For,            // ForStmtNode          for ( <initializer> ; <expr2> ; <expr3> ) <stmt>
            ForIn,          // ForStmtNode          for ( <initializer> in <expr2> ) <stmt>
            Case,           // ExprStmtNode         case <expr> : or default :   (Only occurs directly inside a Switch)
            Break,          // GoStmtNode           break ;   or   break <name> ;
            Continue,       // GoStmtNode           continue ;   or   continue <name>;
            Return,         // ExprStmtNode         return ;   or   return <expr> ;
            Throw,          // ExprStmtNode         throw <expr> ;
            Try,            // TryStmtNode          try <stmt> <catches> <finally>
            Export,         // ExportStmtNode       <attributes> export <bindings> ;
            Const,          // VariableStmtNode     <attributes> const <bindings> ;
            Var,            // VariableStmtNode     <attributes> var <bindings> ;
            Function,       // FunctionStmtNode     <attributes> function <function>
            Class,          // ClassStmtNode        <attributes> class <name> extends <superclass> <body>
            Namespace,      // NamespaceStmtNode    <attributes> namespace <name>
            Use,            // UseStmtNode          use namespace <exprs> ;
            Import,         // ImportStmtNode       import <bindings> ;
            Package,        // PackageStmtNode      package <packageName> <body>
            Pragma,         // PragmaStmtNode       pragma <flags> ;
            Include,        // IncludeStmtNode      include "name" ;
            Debugger        // DebuggerStmtNode     debugger ;
        };

      private:
        Kind kind;          // The node's kind
      public:
        StmtNode *next;     // Next statement in a linked list of statements in this block

        StmtNode(size_t pos, Kind kind): ParseNode(pos), kind(kind), next(0) {}

        Kind getKind() const {return kind;}
        bool hasKind(Kind k) const {return kind == k;}

        static void printStatements(PrettyPrinter &f, const StmtNode *statements);
        static void printBlockStatements(PrettyPrinter &f, const StmtNode *statements, bool loose);
        static void printSemi(PrettyPrinter &f, bool noSemi);
        void printSubstatement(PrettyPrinter &f, bool noSemi, const char *continuation = 0) const;
        virtual void print(PrettyPrinter &f, bool noSemi) const;
    };


    struct ExprStmtNode: StmtNode {
        ExprNode *expr;     // The expression statement's expression. May be nil for default: or return-with-no-expression statements.
        uint32 label;       // Used for case statements' code generation

        ExprStmtNode(size_t pos, Kind kind, ExprNode *expr): StmtNode(pos, kind), expr(expr) {}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct DebuggerStmtNode: StmtNode {
        DebuggerStmtNode(size_t pos, Kind kind): StmtNode(pos, kind) {}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct AttributeStmtNode: StmtNode {
        ExprNode *attributes;           // Block or definition's attributes; nil if none

        JS2Runtime::Attribute *attributeValue;      // used by backend

        AttributeStmtNode(size_t pos, Kind kind, ExprNode *attributes): StmtNode(pos, kind), attributes(attributes) {}

        void printAttributes(PrettyPrinter &f) const;
    };

    struct BlockStmtNode: AttributeStmtNode {
        StmtNode *statements;           // Linked list of block's statements

        BlockStmtNode(size_t pos, Kind kind, ExprNode *attributes, StmtNode *statements):
                AttributeStmtNode(pos, kind, attributes), statements(statements) {}

        void print(PrettyPrinter &f, bool noSemi) const;
        void printBlock(PrettyPrinter &f, bool loose) const;
    };

    struct LabelStmtNode: StmtNode {
        const StringAtom &name;         // The label
        StmtNode *stmt;                 // Labeled statement; non-nil only

        LabelStmtNode(size_t pos, const StringAtom &name, StmtNode *stmt):
                StmtNode(pos, label), name(name), stmt(stmt) {ASSERT(stmt);}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct UnaryStmtNode: ExprStmtNode {
        StmtNode *stmt;                 // First substatement; non-nil only

        UnaryStmtNode(size_t pos, Kind kind, ExprNode *expr, StmtNode *stmt):
                ExprStmtNode(pos, kind, expr), stmt(stmt) {ASSERT(stmt);}

        void print(PrettyPrinter &f, bool noSemi) const;
        virtual void printContents(PrettyPrinter &f, bool noSemi) const;
    };

    struct BinaryStmtNode: UnaryStmtNode {
        StmtNode *stmt2;                // Second substatement; non-nil only

        BinaryStmtNode(size_t pos, Kind kind, ExprNode *expr, StmtNode *stmt1, StmtNode *stmt2):
                UnaryStmtNode(pos, kind, expr, stmt1), stmt2(stmt2) {ASSERT(stmt2);}

        void printContents(PrettyPrinter &f, bool noSemi) const;
    };

    struct ForStmtNode: StmtNode {
        StmtNode *initializer;          // For: First item in parentheses; either nil (if not provided), an expression, or a Var, or a Const.
                                        // ForIn: Expression or declaration before 'in'; either an expression,
                                        //   or a Var or a Const with exactly one binding.
        ExprNode *expr2;                // For: Second item in parentheses; nil if not provided
                                        // ForIn: Subexpression after 'in'; non-nil only
        ExprNode *expr3;                // For: Third item in parentheses; nil if not provided
                                        // ForIn: nil
        StmtNode *stmt;                 // Substatement; non-nil only

        ForStmtNode(size_t pos, Kind kind, StmtNode *initializer, ExprNode *expr2, ExprNode *expr3, StmtNode *stmt):
                StmtNode(pos, kind), initializer(initializer), expr2(expr2), expr3(expr3), stmt(stmt) {ASSERT(stmt);}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct SwitchStmtNode: ExprStmtNode {
        StmtNode *statements;           // Linked list of switch block's statements, which may include Case and Default statements

        SwitchStmtNode(size_t pos, ExprNode *expr, StmtNode *statements):
                ExprStmtNode(pos, Switch, expr), statements(statements) {}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct GoStmtNode: StmtNode {
        const StringAtom *name;         // The label; nil if none

        GoStmtNode(size_t pos, Kind kind, const StringAtom *name): StmtNode(pos, kind), name(name) {}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct CatchClause: ParseNode {
        CatchClause *next;              // Next catch clause in a linked list of catch clauses
        const StringAtom &name;         // The name of the variable that will hold the exception
        ExprNode *type;                 // Type expression or nil if not provided
        bool constant;                  // true for const variables
        StmtNode *stmt;                 // The catch clause's body; non-nil only

        JS2Runtime::Property *prop;     // the sematics/codegen passes stuff their data in here.

        CatchClause(size_t pos, const StringAtom &name, ExprNode *type, bool constant, StmtNode *stmt):
                ParseNode(pos), next(0), name(name), type(type), constant(constant), stmt(stmt) {ASSERT(stmt);}
    };

    struct TryStmtNode: StmtNode {
        StmtNode *stmt;                 // Substatement being tried; usually a block; non-nil only
        CatchClause *catches;           // Linked list of catch blocks; may be nil
        StmtNode *finally;              // Finally block or nil if none

        TryStmtNode(size_t pos, StmtNode *stmt, CatchClause *catches, StmtNode *finally):
                StmtNode(pos, Try), stmt(stmt), catches(catches), finally(finally) {ASSERT(stmt);}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct ExportBinding: ParseNode {
        ExportBinding *next;            // Next binding in a linked list of export bindings
        FunctionName name;              // The exported variable's name; name.name is non-nil only
        FunctionName initializer;       // The original variable's name; same as name if omitted

        ExportBinding(size_t pos): ParseNode(pos), next(0) {}

        void print(PrettyPrinter &f) const;
    };

    struct ExportStmtNode: AttributeStmtNode {
        ExportBinding *bindings;        // Linked list of export bindings; non-nil only

        ExportStmtNode(size_t pos, ExprNode *attributes, ExportBinding *bindings):
                AttributeStmtNode(pos, Export, attributes), bindings(bindings) {ASSERT(bindings);}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct VariableStmtNode: AttributeStmtNode {
        VariableBinding *bindings;      // Linked list of variable bindings; non-nil only

        VariableStmtNode(size_t pos, Kind kind, ExprNode *attributes, VariableBinding *bindings):
                AttributeStmtNode(pos, kind, attributes), bindings(bindings) {ASSERT(bindings);}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct FunctionStmtNode: AttributeStmtNode {
        FunctionDefinition function;    // Function definition
        JS2Runtime::JSFunction *mFunction; // used by backend

        FunctionStmtNode(size_t pos, Kind kind, ExprNode *attributes): AttributeStmtNode(pos, kind, attributes) {}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct NamespaceStmtNode: AttributeStmtNode {
        const StringAtom &name;         // The namespace's or class's name

        NamespaceStmtNode(size_t pos, Kind kind, ExprNode *attributes, const StringAtom &name):
                AttributeStmtNode(pos, kind, attributes), name(name) {}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct ClassStmtNode: NamespaceStmtNode {
        ExprNode *superclass;           // Superclass expression (classes only); nil if omitted
        BlockStmtNode *body;            // The class's body; nil if omitted

        JS2Runtime::JSType *mType;      // used by backend

        ClassStmtNode(size_t pos, ExprNode *attributes, const StringAtom &name, ExprNode *superclass, BlockStmtNode *body):
                NamespaceStmtNode(pos, Class, attributes, name), superclass(superclass), body(body) {}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct IdentifierList: ArenaObject {
        IdentifierList *next;           // Next identifier in linked list
        const StringAtom &name;         // The identifier

        explicit IdentifierList(const StringAtom &name): next(0), name(name) {}

        void printList(PrettyPrinter &f, char separator) const;
    };

    struct ExprList: ArenaObject {
        ExprList *next;                 // Next expression in linked list
        ExprNode *expr;                 // Expression; non-nil only

        explicit ExprList(ExprNode *expr): next(0), expr(expr) {ASSERT(expr);}

        void printList(PrettyPrinter &f) const;
    };

    struct UseStmtNode: AttributeStmtNode {
        ExprList *namespaces;           // Linked list of namespace expressions; may be nil in an import statement
        IdentifierList *includeExclude; // Linked list of include/exclude identifiers
        bool exclude;                   // If true, includeExclude lists excluded identifiers; if false, included identifiers

        UseStmtNode(size_t pos, Kind kind, ExprNode *attributes, ExprList *namespaces):
                AttributeStmtNode(pos, kind, attributes), namespaces(namespaces) {}

      protected:
        void printNamespaces(PrettyPrinter &f) const;
        void printIncludeExclude(PrettyPrinter &f) const;
      public:
        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct ImportStmtNode: UseStmtNode {
        const StringAtom *varName;      // The package variable's name; nil if omitted
        // Either packageIdList or packageString may be nil, but not both
        IdentifierList *packageIdList;  // The package name as a list of identifiers
        String *packageString;          // The package name as a string

        ImportStmtNode(size_t pos, ExprNode *attributes, const StringAtom *varName, IdentifierList *packageIdList,
                       String *packageString, ExprList *namespaces):
                UseStmtNode(pos, Import, attributes, namespaces), varName(varName), packageIdList(packageIdList),
                packageString(packageString) {}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct PackageStmtNode: StmtNode {
        IdentifierList *packageIdList;  // The package name as a list of identifiers; may be nil
        BlockStmtNode *body;            // The package's body; non-nil only

        PackageStmtNode(size_t pos, IdentifierList *packageIdList, BlockStmtNode *body):
                StmtNode(pos, Package), packageIdList(packageIdList), body(body) {ASSERT(body);}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct PragmaStmtNode: StmtNode {
        Pragma::Flags flags;            // The new set of flags to use until the next PragmaStmtNode or the end of this scope

        PragmaStmtNode(size_t pos, Pragma::Flags flags): StmtNode(pos, Pragma), flags(flags) {}

        void print(PrettyPrinter &f, bool noSemi) const;
    };

    struct IncludeStmtNode: StmtNode {
        String &name;                   // The file name

        IncludeStmtNode(size_t pos, String &name): StmtNode(pos, Include), name(name) {}

        void print(PrettyPrinter &f, bool noSemi) const;
    };


    class Parser {
      public:
        Lexer lexer;
        Arena &arena;
        Pragma::Flags &flags;           // The flags currently guiding the parser; saved and restored on entering nested scopes

        Parser(World &world, Arena &arena, Pragma::Flags &flags, const String &source, const String &sourceLocation, uint32 initialLineNum = 1);

      private:
        Reader &getReader() {return lexer.reader;}
        World &getWorld() {return lexer.world;}

      public:
        void syntaxError(const char *message, uint backUp = 1);
        void syntaxError(const String &message, uint backUp = 1);
        const Token &require(bool preferRegExp, Token::Kind kind);
      private:
        String &copyTokenChars(const Token &t);
        bool lineBreakBefore(const Token &t) const {return Pragma::lineBreaksSignificant(flags) && t.getLineBreak();}
        bool lineBreakBefore() {return Pragma::lineBreaksSignificant(flags) && lexer.peek(true).getLineBreak();}

        enum SuperState {
            ssNone,             // No super operator
            ssExpr,             // super or super(expr)
            ssStmt              // super or super(expr) or super(arguments)
        };

        enum Precedence {
            pNone,              // End tag
            pExpression,        // ListExpression
            pAssignment,        // AssignmentExpression
            pConditional,       // ConditionalExpression
            pLogicalOr,         // LogicalOrExpression
            pLogicalXor,        // LogicalXorExpression
            pLogicalAnd,        // LogicalAndExpression
            pBitwiseOr,         // BitwiseOrExpression
            pBitwiseXor,        // BitwiseXorExpression
            pBitwiseAnd,        // BitwiseAndExpression
            pEquality,          // EqualityExpression
            pRelational,        // RelationalExpression
            pShift,             // ShiftExpression
            pAdditive,          // AdditiveExpression
            pMultiplicative,    // MultiplicativeExpression
            pUnary,             // UnaryExpression
            pPostfix            // PostfixExpression
        };

        struct BinaryOperatorInfo {
            ExprNode::Kind kind;        // The kind of BinaryExprNode the operator should generate;
                                        // ExprNode::none if not a binary operator
            Precedence precedenceLeft;  // Operators in this operator's left subexpression with precedenceLeft or higher are reduced
            Precedence precedenceRight; // This operator's precedence
            bool superLeft;             // True if the left operand can be a SuperExpression
        };

        static const BinaryOperatorInfo tokenBinaryOperatorInfos[Token::kindsEnd];
        struct StackedSubexpression;

        ExprNode *makeIdentifierExpression(const Token &t) const;
        const StringAtom &ensureIdentifier(const Token &t);
        const StringAtom &parseIdentifier();
        IdentifierList *parseIdentifierList(Token::Kind separator);
        ExprNode *parseIdentifierQualifiers(ExprNode *e, bool &foundQualifiers, bool preferRegExp);
        ExprNode *parseParenthesesAndIdentifierQualifiers(const Token &tParen, bool noComma, bool &foundQualifiers, bool preferRegExp);
        ExprNode *parseQualifiedIdentifier(const Token &t, bool preferRegExp);
        PairListExprNode *parseArrayLiteral(const Token &initialToken);
        PairListExprNode *parseObjectLiteral(const Token &initialToken);
        ExprNode *parseUnitSuffixes(ExprNode *e);
        ExprNode *parseSuper(size_t pos, SuperState superState);
      public:
        FunctionExprNode *parseFunctionExpression(size_t pos);
      private:
        ExprNode *parsePrimaryExpression(SuperState superState);
        ExprNode *parseMember(ExprNode *target, const Token &tOperator, bool preferRegExp);
        InvokeExprNode *parseInvoke(ExprNode *target, size_t pos, Token::Kind closingTokenKind, ExprNode::Kind invokeKind);
        ExprNode *parsePostfixOperator(ExprNode *e, bool newExpression, bool attribute);
        ExprNode *parsePostfixExpression(SuperState superState, bool newExpression);
        void ensurePostfix(const ExprNode *e);
        ExprNode *parseUnaryExpression(SuperState superState);
        ExprNode *parseGeneralExpression(bool allowSuperStmt, bool noIn, bool noAssignment, bool noComma);
      public:
        ExprNode *parseListExpression(bool noIn) {return parseGeneralExpression(false, noIn, false, false);}
        ExprNode *parseAssignmentExpression(bool noIn) {return parseGeneralExpression(false, noIn, false, true);}
        ExprNode *parseNonAssignmentExpression(bool noIn) {return parseGeneralExpression(false, noIn, true, true);}

      private:
        ExprNode *parseAttribute(const Token &t);
        ExprNode *parseAttributes(size_t pos, ExprNode *attribute);
        static bool expressionIsAttribute(const ExprNode *e);
        ExprNode *parseParenthesizedListExpression();
        ExprList *parseParenthesizedExpressionList(bool optional);
        ExprNode *parseTypeExpression(bool noIn=false);
        ExprNode *parseTypeBinding(Token::Kind kind, bool noIn);
        bool doubleColonFollows();
        VariableBinding *parseVariableBinding(size_t pos, bool noIn, bool untyped, bool constant);
        VariableBinding *parseParameter();
        void parseFunctionName(FunctionName &fn);
        void parseFunctionSignature(FunctionDefinition &fd);
        ExportBinding *parseExportBinding();
        StmtNode *parseBlockContents(bool inSwitch);
        BlockStmtNode *parseBody(bool *semicolonWanted);
        void parseIncludeExclude(UseStmtNode &s);
        ImportStmtNode *parseImport(size_t pos, ExprNode *attributes);
        UseStmtNode *parseUseDirective(size_t pos, ExprNode *attributes);
        void parseClosingSemicolon();
        bool parseBooleanPragma(const ExprList *arguments, bool &value, bool ignoreErrors);
        bool parseNumericPragma(const ExprList *arguments, float64 &value, bool ignoreErrors);
        StmtNode *parseUse(size_t pos, ExprNode *attributes, bool &semicolonWanted);
        StmtNode *parseAnnotatableDirective(size_t pos, ExprNode *attributes, const Token &t, bool noIn, bool untyped, bool &semicolonWanted);
        ForStmtNode *parseFor(size_t pos, bool &semicolonWanted);
        TryStmtNode *parseTry(size_t pos);
        PackageStmtNode *parsePackage(size_t pos);
        StmtNode *parseDirective(bool substatement, bool inSwitch, bool &semicolonWanted);
        StmtNode *parseSubstatement(bool &semicolonWanted);

      public:
        StmtNode *parseFullDirective(bool inSwitch);
        StmtNode *parseProgram();
    };
}
#endif /* parser_h___ */
