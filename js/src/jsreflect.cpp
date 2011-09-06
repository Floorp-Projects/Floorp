/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * June 12, 2009.
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
 *
 * Contributor(s):
 *   Dave Herman <dherman@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * JS reflection package.
 */
#include <stdlib.h>
#include <string.h>     /* for jsparse.h */
#include "jspubtd.h"
#include "jsatom.h"
#include "jsobj.h"
#include "jsreflect.h"
#include "jscntxt.h"    /* for jsparse.h */
#include "jsbit.h"      /* for jsparse.h */
#include "jsscript.h"   /* for jsparse.h */
#include "jsinterp.h"   /* for jsparse.h */
#include "jsparse.h"
#include "jsregexp.h"
#include "jsvector.h"
#include "jsemit.h"
#include "jsscan.h"
#include "jsprf.h"
#include "jsiter.h"
#include "jsbool.h"
#include "jsval.h"
#include "jsvalue.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsobj.h"
#include "jsarray.h"
#include "jsnum.h"

#include "jsscriptinlines.h"

using namespace js;

namespace js {

char const *aopNames[] = {
    "=",    /* AOP_ASSIGN */
    "+=",   /* AOP_PLUS */
    "-=",   /* AOP_MINUS */
    "*=",   /* AOP_STAR */
    "/=",   /* AOP_DIV */
    "%=",   /* AOP_MOD */
    "<<=",  /* AOP_LSH */
    ">>=",  /* AOP_RSH */
    ">>>=", /* AOP_URSH */
    "|=",   /* AOP_BITOR */
    "^=",   /* AOP_BITXOR */
    "&="    /* AOP_BITAND */
};

char const *binopNames[] = {
    "==",         /* BINOP_EQ */
    "!=",         /* BINOP_NE */
    "===",        /* BINOP_STRICTEQ */
    "!==",        /* BINOP_STRICTNE */
    "<",          /* BINOP_LT */
    "<=",         /* BINOP_LE */
    ">",          /* BINOP_GT */
    ">=",         /* BINOP_GE */
    "<<",         /* BINOP_LSH */
    ">>",         /* BINOP_RSH */
    ">>>",        /* BINOP_URSH */
    "+",          /* BINOP_PLUS */
    "-",          /* BINOP_MINUS */
    "*",          /* BINOP_STAR */
    "/",          /* BINOP_DIV */
    "%",          /* BINOP_MOD */
    "|",          /* BINOP_BITOR */
    "^",          /* BINOP_BITXOR */
    "&",          /* BINOP_BITAND */
    "in",         /* BINOP_IN */
    "instanceof", /* BINOP_INSTANCEOF */
    "..",         /* BINOP_DBLDOT */
};

char const *unopNames[] = {
    "delete",  /* UNOP_DELETE */
    "-",       /* UNOP_NEG */
    "+",       /* UNOP_POS */
    "!",       /* UNOP_NOT */
    "~",       /* UNOP_BITNOT */
    "typeof",  /* UNOP_TYPEOF */
    "void"     /* UNOP_VOID */
};

char const *nodeTypeNames[] = {
#define ASTDEF(ast, str, method) str,
#include "jsast.tbl"
#undef ASTDEF
    NULL
};

char const *callbackNames[] = {
#define ASTDEF(ast, str, method) method,
#include "jsast.tbl"
#undef ASTDEF
    NULL
};

typedef AutoValueVector NodeVector;

/*
 * JSParseNode is a somewhat intricate data structure, and its invariants have
 * evolved, making it more likely that there could be a disconnect between the
 * parser and the AST serializer. We use these macros to check invariants on a
 * parse node and raise a dynamic error on failure.
 */
#define LOCAL_ASSERT(expr)                                                             \
    JS_BEGIN_MACRO                                                                     \
        JS_ASSERT(expr);                                                               \
        if (!(expr)) {                                                                 \
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_PARSE_NODE);  \
            return false;                                                              \
        }                                                                              \
    JS_END_MACRO

#define LOCAL_NOT_REACHED(expr)                                                        \
    JS_BEGIN_MACRO                                                                     \
        JS_NOT_REACHED(expr);                                                          \
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_PARSE_NODE);      \
        return false;                                                                  \
    JS_END_MACRO


/*
 * Builder class that constructs JavaScript AST node objects. See:
 *
 *     https://developer.mozilla.org/en/SpiderMonkey/Parser_API
 *
 * Bug 569487: generalize builder interface
 */
class NodeBuilder
{
    JSContext   *cx;
    bool        saveLoc;               /* save source location information?     */
    char const  *src;                  /* source filename or null               */
    Value       srcval;                /* source filename JS value or null      */
    Value       callbacks[AST_LIMIT];  /* user-specified callbacks              */
    Value       userv;                 /* user-specified builder object or null */

  public:
    NodeBuilder(JSContext *c, bool l, char const *s)
        : cx(c), saveLoc(l), src(s) {
    }

    bool init(JSObject *userobj = NULL) {
        if (src) {
            if (!atomValue(src, &srcval))
                return false;
        } else {
            srcval.setNull();
        }

        if (!userobj) {
            userv.setNull();
            for (uintN i = 0; i < AST_LIMIT; i++) {
                callbacks[i].setNull();
            }
            return true;
        }

        userv.setObject(*userobj);

        for (uintN i = 0; i < AST_LIMIT; i++) {
            Value funv;

            const char *name = callbackNames[i];
            JSAtom *atom = js_Atomize(cx, name, strlen(name));
            if (!atom || !GetPropertyDefault(cx, userobj, ATOM_TO_JSID(atom), NullValue(), &funv))
                return false;

            if (funv.isNullOrUndefined()) {
                callbacks[i].setNull();
                continue;
            }

            if (!funv.isObject() || !funv.toObject().isFunction()) {
                js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_NOT_FUNCTION,
                                         JSDVG_SEARCH_STACK, funv, NULL, NULL, NULL);
                return false;
            }

            callbacks[i] = funv;
        }

        return true;
    }

  private:
    bool callback(Value fun, TokenPos *pos, Value *dst) {
        if (saveLoc) {
            Value loc;
            if (!newNodeLoc(pos, &loc))
                return false;
            Value argv[] = { loc };
            return Invoke(cx, userv, fun, JS_ARRAY_LENGTH(argv), argv, dst);
        }

        Value argv[] = { NullValue() }; /* no zero-length arrays allowed! */
        return Invoke(cx, userv, fun, 0, argv, dst);
    }

    bool callback(Value fun, Value v1, TokenPos *pos, Value *dst) {
        if (saveLoc) {
            Value loc;
            if (!newNodeLoc(pos, &loc))
                return false;
            Value argv[] = { v1, loc };
            return Invoke(cx, userv, fun, JS_ARRAY_LENGTH(argv), argv, dst);
        }

        Value argv[] = { v1 };
        return Invoke(cx, userv, fun, JS_ARRAY_LENGTH(argv), argv, dst);
    }

    bool callback(Value fun, Value v1, Value v2, TokenPos *pos, Value *dst) {
        if (saveLoc) {
            Value loc;
            if (!newNodeLoc(pos, &loc))
                return false;
            Value argv[] = { v1, v2, loc };
            return Invoke(cx, userv, fun, JS_ARRAY_LENGTH(argv), argv, dst);
        }

        Value argv[] = { v1, v2 };
        return Invoke(cx, userv, fun, JS_ARRAY_LENGTH(argv), argv, dst);
    }

    bool callback(Value fun, Value v1, Value v2, Value v3, TokenPos *pos, Value *dst) {
        if (saveLoc) {
            Value loc;
            if (!newNodeLoc(pos, &loc))
                return false;
            Value argv[] = { v1, v2, v3, loc };
            return Invoke(cx, userv, fun, JS_ARRAY_LENGTH(argv), argv, dst);
        }

        Value argv[] = { v1, v2, v3 };
        return Invoke(cx, userv, fun, JS_ARRAY_LENGTH(argv), argv, dst);
    }

    bool callback(Value fun, Value v1, Value v2, Value v3, Value v4, TokenPos *pos, Value *dst) {
        if (saveLoc) {
            Value loc;
            if (!newNodeLoc(pos, &loc))
                return false;
            Value argv[] = { v1, v2, v3, v4, loc };
            return Invoke(cx, userv, fun, JS_ARRAY_LENGTH(argv), argv, dst);
        }

        Value argv[] = { v1, v2, v3, v4 };
        return Invoke(cx, userv, fun, JS_ARRAY_LENGTH(argv), argv, dst);
    }

    bool callback(Value fun, Value v1, Value v2, Value v3, Value v4, Value v5,
                  TokenPos *pos, Value *dst) {
        if (saveLoc) {
            Value loc;
            if (!newNodeLoc(pos, &loc))
                return false;
            Value argv[] = { v1, v2, v3, v4, v5, loc };
            return Invoke(cx, userv, fun, JS_ARRAY_LENGTH(argv), argv, dst);
        }

        Value argv[] = { v1, v2, v3, v4, v5 };
        return Invoke(cx, userv, fun, JS_ARRAY_LENGTH(argv), argv, dst);
    }

    Value opt(Value v) {
        JS_ASSERT_IF(v.isMagic(), v.whyMagic() == JS_SERIALIZE_NO_NODE);
        return v.isMagic(JS_SERIALIZE_NO_NODE) ? UndefinedValue() : v;
    }

    bool atomValue(const char *s, Value *dst) {
        /*
         * Bug 575416: instead of js_Atomize, lookup constant atoms in tbl file
         */
        JSAtom *atom = js_Atomize(cx, s, strlen(s));
        if (!atom)
            return false;

        dst->setString(atom);
        return true;
    }

    bool newObject(JSObject **dst) {
        JSObject *nobj = NewNonFunction<WithProto::Class>(cx, &ObjectClass, NULL, NULL);
        if (!nobj)
            return false;

        *dst = nobj;
        return true;
    }

    bool newArray(NodeVector &elts, Value *dst);

    bool newNode(ASTType type, TokenPos *pos, JSObject **dst);

    bool newNode(ASTType type, TokenPos *pos, Value *dst) {
        JSObject *node;
        return newNode(type, pos, &node) &&
               setResult(node, dst);
    }

    bool newNode(ASTType type, TokenPos *pos, const char *childName, Value child, Value *dst) {
        JSObject *node;
        return newNode(type, pos, &node) &&
               setProperty(node, childName, child) &&
               setResult(node, dst);
    }

    bool newNode(ASTType type, TokenPos *pos,
                 const char *childName1, Value child1,
                 const char *childName2, Value child2,
                 Value *dst) {
        JSObject *node;
        return newNode(type, pos, &node) &&
               setProperty(node, childName1, child1) &&
               setProperty(node, childName2, child2) &&
               setResult(node, dst);
    }

    bool newNode(ASTType type, TokenPos *pos,
                 const char *childName1, Value child1,
                 const char *childName2, Value child2,
                 const char *childName3, Value child3,
                 Value *dst) {
        JSObject *node;
        return newNode(type, pos, &node) &&
               setProperty(node, childName1, child1) &&
               setProperty(node, childName2, child2) &&
               setProperty(node, childName3, child3) &&
               setResult(node, dst);
    }

    bool newNode(ASTType type, TokenPos *pos,
                 const char *childName1, Value child1,
                 const char *childName2, Value child2,
                 const char *childName3, Value child3,
                 const char *childName4, Value child4,
                 Value *dst) {
        JSObject *node;
        return newNode(type, pos, &node) &&
               setProperty(node, childName1, child1) &&
               setProperty(node, childName2, child2) &&
               setProperty(node, childName3, child3) &&
               setProperty(node, childName4, child4) &&
               setResult(node, dst);
    }

    bool newNode(ASTType type, TokenPos *pos,
                 const char *childName1, Value child1,
                 const char *childName2, Value child2,
                 const char *childName3, Value child3,
                 const char *childName4, Value child4,
                 const char *childName5, Value child5,
                 Value *dst) {
        JSObject *node;
        return newNode(type, pos, &node) &&
               setProperty(node, childName1, child1) &&
               setProperty(node, childName2, child2) &&
               setProperty(node, childName3, child3) &&
               setProperty(node, childName4, child4) &&
               setProperty(node, childName5, child5) &&
               setResult(node, dst);
    }

    bool listNode(ASTType type, const char *propName, NodeVector &elts, TokenPos *pos, Value *dst) {
        Value array;
        if (!newArray(elts, &array))
            return false;

        Value cb = callbacks[type];
        if (!cb.isNull())
            return callback(cb, array, pos, dst);

        return newNode(type, pos, propName, array, dst);
    }

    bool setProperty(JSObject *obj, const char *name, Value val) {
        JS_ASSERT_IF(val.isMagic(), val.whyMagic() == JS_SERIALIZE_NO_NODE);

        /* Represent "no node" as null and ensure users are not exposed to magic values. */
        if (val.isMagic(JS_SERIALIZE_NO_NODE))
            val.setNull();

        /*
         * Bug 575416: instead of js_Atomize, lookup constant atoms in tbl file
         */
        JSAtom *atom = js_Atomize(cx, name, strlen(name));
        if (!atom)
            return false;

        return obj->defineProperty(cx, ATOM_TO_JSID(atom), val);
    }

    bool newNodeLoc(TokenPos *pos, Value *dst);

    bool setNodeLoc(JSObject *obj, TokenPos *pos);

    bool setResult(JSObject *obj, Value *dst) {
        JS_ASSERT(obj);
        dst->setObject(*obj);
        return true;
    }

  public:
    /*
     * All of the public builder methods take as their last two
     * arguments a nullable token position and a non-nullable, rooted
     * outparam.
     *
     * All Value arguments are rooted. Any Value arguments representing
     * optional subnodes may be a JS_SERIALIZE_NO_NODE magic value.
     */

    /*
     * misc nodes
     */

    bool program(NodeVector &elts, TokenPos *pos, Value *dst);

    bool literal(Value val, TokenPos *pos, Value *dst);

    bool identifier(Value name, TokenPos *pos, Value *dst);

    bool function(ASTType type, TokenPos *pos,
                  Value id, NodeVector &args, Value body,
                  bool isGenerator, bool isExpression, Value *dst);

    bool variableDeclarator(Value id, Value init, TokenPos *pos, Value *dst);

    bool switchCase(Value expr, NodeVector &elts, TokenPos *pos, Value *dst);

    bool catchClause(Value var, Value guard, Value body, TokenPos *pos, Value *dst);

    bool propertyInitializer(Value key, Value val, PropKind kind, TokenPos *pos, Value *dst);


    /*
     * statements
     */

    bool blockStatement(NodeVector &elts, TokenPos *pos, Value *dst);

    bool expressionStatement(Value expr, TokenPos *pos, Value *dst);

    bool emptyStatement(TokenPos *pos, Value *dst);

    bool ifStatement(Value test, Value cons, Value alt, TokenPos *pos, Value *dst);

    bool breakStatement(Value label, TokenPos *pos, Value *dst);

    bool continueStatement(Value label, TokenPos *pos, Value *dst);

    bool labeledStatement(Value label, Value stmt, TokenPos *pos, Value *dst);

    bool throwStatement(Value arg, TokenPos *pos, Value *dst);

    bool returnStatement(Value arg, TokenPos *pos, Value *dst);

    bool forStatement(Value init, Value test, Value update, Value stmt,
                      TokenPos *pos, Value *dst);

    bool forInStatement(Value var, Value expr, Value stmt,
                        bool isForEach, TokenPos *pos, Value *dst);

    bool withStatement(Value expr, Value stmt, TokenPos *pos, Value *dst);

    bool whileStatement(Value test, Value stmt, TokenPos *pos, Value *dst);

    bool doWhileStatement(Value stmt, Value test, TokenPos *pos, Value *dst);

    bool switchStatement(Value disc, NodeVector &elts, bool lexical, TokenPos *pos, Value *dst);

    bool tryStatement(Value body, NodeVector &catches, Value finally, TokenPos *pos, Value *dst);

    bool debuggerStatement(TokenPos *pos, Value *dst);

    bool letStatement(NodeVector &head, Value stmt, TokenPos *pos, Value *dst);

    /*
     * expressions
     */

    bool binaryExpression(BinaryOperator op, Value left, Value right, TokenPos *pos, Value *dst);

    bool unaryExpression(UnaryOperator op, Value expr, TokenPos *pos, Value *dst);

    bool assignmentExpression(AssignmentOperator op, Value lhs, Value rhs,
                              TokenPos *pos, Value *dst);

    bool updateExpression(Value expr, bool incr, bool prefix, TokenPos *pos, Value *dst);

    bool logicalExpression(bool lor, Value left, Value right, TokenPos *pos, Value *dst);

    bool conditionalExpression(Value test, Value cons, Value alt, TokenPos *pos, Value *dst);

    bool sequenceExpression(NodeVector &elts, TokenPos *pos, Value *dst);

    bool newExpression(Value callee, NodeVector &args, TokenPos *pos, Value *dst);

    bool callExpression(Value callee, NodeVector &args, TokenPos *pos, Value *dst);

    bool memberExpression(bool computed, Value expr, Value member, TokenPos *pos, Value *dst);

    bool arrayExpression(NodeVector &elts, TokenPos *pos, Value *dst);

    bool objectExpression(NodeVector &elts, TokenPos *pos, Value *dst);

    bool thisExpression(TokenPos *pos, Value *dst);

    bool yieldExpression(Value arg, TokenPos *pos, Value *dst);

    bool comprehensionBlock(Value patt, Value src, bool isForEach, TokenPos *pos, Value *dst);

    bool comprehensionExpression(Value body, NodeVector &blocks, Value filter,
                                 TokenPos *pos, Value *dst);

    bool generatorExpression(Value body, NodeVector &blocks, Value filter,
                             TokenPos *pos, Value *dst);

    bool graphExpression(jsint idx, Value expr, TokenPos *pos, Value *dst);

    bool graphIndexExpression(jsint idx, TokenPos *pos, Value *dst);

    bool letExpression(NodeVector &head, Value expr, TokenPos *pos, Value *dst);

    /*
     * declarations
     */

    bool variableDeclaration(NodeVector &elts, VarDeclKind kind, TokenPos *pos, Value *dst);

    /*
     * patterns
     */

    bool arrayPattern(NodeVector &elts, TokenPos *pos, Value *dst);

    bool objectPattern(NodeVector &elts, TokenPos *pos, Value *dst);

    bool propertyPattern(Value key, Value patt, TokenPos *pos, Value *dst);

    /*
     * xml
     */

    bool xmlAnyName(TokenPos *pos, Value *dst);

    bool xmlEscapeExpression(Value expr, TokenPos *pos, Value *dst);

    bool xmlDefaultNamespace(Value ns, TokenPos *pos, Value *dst);

    bool xmlFilterExpression(Value left, Value right, TokenPos *pos, Value *dst);

    bool xmlAttributeSelector(Value expr, bool computed, TokenPos *pos, Value *dst);

    bool xmlQualifiedIdentifier(Value left, Value right, bool computed, TokenPos *pos, Value *dst);

    bool xmlFunctionQualifiedIdentifier(Value right, bool computed, TokenPos *pos, Value *dst);

    bool xmlElement(NodeVector &elts, TokenPos *pos, Value *dst);

    bool xmlText(Value text, TokenPos *pos, Value *dst);

    bool xmlList(NodeVector &elts, TokenPos *pos, Value *dst);

    bool xmlStartTag(NodeVector &elts, TokenPos *pos, Value *dst);

    bool xmlEndTag(NodeVector &elts, TokenPos *pos, Value *dst);

    bool xmlPointTag(NodeVector &elts, TokenPos *pos, Value *dst);

    bool xmlName(Value text, TokenPos *pos, Value *dst);

    bool xmlName(NodeVector &elts, TokenPos *pos, Value *dst);

    bool xmlAttribute(Value text, TokenPos *pos, Value *dst);

    bool xmlCdata(Value text, TokenPos *pos, Value *dst);

    bool xmlComment(Value text, TokenPos *pos, Value *dst);

    bool xmlPI(Value target, TokenPos *pos, Value *dst);

    bool xmlPI(Value target, Value content, TokenPos *pos, Value *dst);
};

bool
NodeBuilder::newNode(ASTType type, TokenPos *pos, JSObject **dst)
{
    JS_ASSERT(type > AST_ERROR && type < AST_LIMIT);

    Value tv;

    JSObject *node = NewNonFunction<WithProto::Class>(cx, &ObjectClass, NULL, NULL);
    if (!node ||
        !setNodeLoc(node, pos) ||
        !atomValue(nodeTypeNames[type], &tv) ||
        !setProperty(node, "type", tv)) {
        return false;
    }

    *dst = node;
    return true;
}

bool
NodeBuilder::newArray(NodeVector &elts, Value *dst)
{
    JSObject *array = NewDenseEmptyArray(cx);
    if (!array)
        return false;

    const size_t len = elts.length();
    for (size_t i = 0; i < len; i++) {
        Value val = elts[i];

        JS_ASSERT_IF(val.isMagic(), val.whyMagic() == JS_SERIALIZE_NO_NODE);

        /* Represent "no node" as an array hole by not adding the value. */
        if (val.isMagic(JS_SERIALIZE_NO_NODE))
            continue;

        if (!array->setProperty(cx, INT_TO_JSID(i), &val, false))
            return false;
    }

    dst->setObject(*array);
    return true;
}

bool
NodeBuilder::newNodeLoc(TokenPos *pos, Value *dst)
{
    if (!pos) {
        dst->setNull();
        return true;
    }
 
    JSObject *loc, *to;
    Value tv;

    if (!newObject(&loc))
        return false;

    dst->setObject(*loc);

    return newObject(&to) &&
           setProperty(loc, "start", ObjectValue(*to)) &&
           (tv.setNumber(pos->begin.lineno), true) &&
           setProperty(to, "line", tv) &&
           (tv.setNumber(pos->begin.index), true) &&
           setProperty(to, "column", tv) &&

           newObject(&to) &&
           setProperty(loc, "end", ObjectValue(*to)) &&
           (tv.setNumber(pos->end.lineno), true) &&
           setProperty(to, "line", tv) &&
           (tv.setNumber(pos->end.index), true) &&
           setProperty(to, "column", tv) &&

           setProperty(loc, "source", srcval);
}

bool
NodeBuilder::setNodeLoc(JSObject *node, TokenPos *pos)
{
    if (!saveLoc) {
        setProperty(node, "loc", NullValue());
        return true;
    }

    Value loc;
    return newNodeLoc(pos, &loc) &&
           setProperty(node, "loc", loc);
}

bool
NodeBuilder::program(NodeVector &elts, TokenPos *pos, Value *dst)
{
    return listNode(AST_PROGRAM, "body", elts, pos, dst);
}

bool
NodeBuilder::blockStatement(NodeVector &elts, TokenPos *pos, Value *dst)
{
    return listNode(AST_BLOCK_STMT, "body", elts, pos, dst);
}

bool
NodeBuilder::expressionStatement(Value expr, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_EXPR_STMT];
    if (!cb.isNull())
        return callback(cb, expr, pos, dst);

    return newNode(AST_EXPR_STMT, pos, "expression", expr, dst);
}

bool
NodeBuilder::emptyStatement(TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_EMPTY_STMT];
    if (!cb.isNull())
        return callback(cb, pos, dst);

    return newNode(AST_EMPTY_STMT, pos, dst);
}

bool
NodeBuilder::ifStatement(Value test, Value cons, Value alt, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_IF_STMT];
    if (!cb.isNull())
        return callback(cb, test, cons, opt(alt), pos, dst);

    return newNode(AST_IF_STMT, pos,
                   "test", test,
                   "consequent", cons,
                   "alternate", alt,
                   dst);
}

bool
NodeBuilder::breakStatement(Value label, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_BREAK_STMT];
    if (!cb.isNull())
        return callback(cb, opt(label), pos, dst);

    return newNode(AST_BREAK_STMT, pos, "label", label, dst);
}

bool
NodeBuilder::continueStatement(Value label, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_CONTINUE_STMT];
    if (!cb.isNull())
        return callback(cb, opt(label), pos, dst);

    return newNode(AST_CONTINUE_STMT, pos, "label", label, dst);
}

bool
NodeBuilder::labeledStatement(Value label, Value stmt, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_LAB_STMT];
    if (!cb.isNull())
        return callback(cb, label, stmt, pos, dst);

    return newNode(AST_LAB_STMT, pos,
                   "label", label,
                   "body", stmt,
                   dst);
}

bool
NodeBuilder::throwStatement(Value arg, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_THROW_STMT];
    if (!cb.isNull())
        return callback(cb, arg, pos, dst);

    return newNode(AST_THROW_STMT, pos, "argument", arg, dst);
}

bool
NodeBuilder::returnStatement(Value arg, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_RETURN_STMT];
    if (!cb.isNull())
        return callback(cb, opt(arg), pos, dst);

    return newNode(AST_RETURN_STMT, pos, "argument", arg, dst);
}

bool
NodeBuilder::forStatement(Value init, Value test, Value update, Value stmt,
                          TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_FOR_STMT];
    if (!cb.isNull())
        return callback(cb, opt(init), opt(test), opt(update), stmt, pos, dst);

    return newNode(AST_FOR_STMT, pos,
                   "init", init,
                   "test", test,
                   "update", update,
                   "body", stmt,
                   dst);
}

bool
NodeBuilder::forInStatement(Value var, Value expr, Value stmt, bool isForEach,
                            TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_FOR_IN_STMT];
    if (!cb.isNull())
        return callback(cb, var, expr, stmt, BooleanValue(isForEach), pos, dst);

    return newNode(AST_FOR_IN_STMT, pos,
                   "left", var,
                   "right", expr,
                   "body", stmt,
                   "each", BooleanValue(isForEach),
                   dst);
}

bool
NodeBuilder::withStatement(Value expr, Value stmt, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_WITH_STMT];
    if (!cb.isNull())
        return callback(cb, expr, stmt, pos, dst);

    return newNode(AST_WITH_STMT, pos,
                   "object", expr,
                   "body", stmt,
                   dst);
}

bool
NodeBuilder::whileStatement(Value test, Value stmt, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_WHILE_STMT];
    if (!cb.isNull())
        return callback(cb, test, stmt, pos, dst);

    return newNode(AST_WHILE_STMT, pos,
                   "test", test,
                   "body", stmt,
                   dst);
}

bool
NodeBuilder::doWhileStatement(Value stmt, Value test, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_DO_STMT];
    if (!cb.isNull())
        return callback(cb, stmt, test, pos, dst);

    return newNode(AST_DO_STMT, pos,
                   "body", stmt,
                   "test", test,
                   dst);
}

bool
NodeBuilder::switchStatement(Value disc, NodeVector &elts, bool lexical, TokenPos *pos, Value *dst)
{
    Value array;
    if (!newArray(elts, &array))
        return false;

    Value cb = callbacks[AST_SWITCH_STMT];
    if (!cb.isNull())
        return callback(cb, disc, array, BooleanValue(lexical), pos, dst);

    return newNode(AST_SWITCH_STMT, pos,
                   "discriminant", disc,
                   "cases", array,
                   "lexical", BooleanValue(lexical),
                   dst);
}

bool
NodeBuilder::tryStatement(Value body, NodeVector &catches, Value finally,
                          TokenPos *pos, Value *dst)
{
    Value handlers;

    Value cb = callbacks[AST_TRY_STMT];
    if (!cb.isNull()) {
        return newArray(catches, &handlers) &&
               callback(cb, body, handlers, opt(finally), pos, dst);
    }

    if (!newArray(catches, &handlers))
        return false;

    return newNode(AST_TRY_STMT, pos,
                   "block", body,
                   "handlers", handlers,
                   "finalizer", finally,
                   dst);
}

bool
NodeBuilder::debuggerStatement(TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_DEBUGGER_STMT];
    if (!cb.isNull())
        return callback(cb, pos, dst);

    return newNode(AST_DEBUGGER_STMT, pos, dst);
}

bool
NodeBuilder::binaryExpression(BinaryOperator op, Value left, Value right, TokenPos *pos, Value *dst)
{
    JS_ASSERT(op > BINOP_ERR && op < BINOP_LIMIT);

    Value opName;
    if (!atomValue(binopNames[op], &opName))
        return false;

    Value cb = callbacks[AST_BINARY_EXPR];
    if (!cb.isNull())
        return callback(cb, opName, left, right, pos, dst);

    return newNode(AST_BINARY_EXPR, pos,
                   "operator", opName,
                   "left", left,
                   "right", right,
                   dst);
}

bool
NodeBuilder::unaryExpression(UnaryOperator unop, Value expr, TokenPos *pos, Value *dst)
{
    JS_ASSERT(unop > UNOP_ERR && unop < UNOP_LIMIT);

    Value opName;
    if (!atomValue(unopNames[unop], &opName))
        return false;

    Value cb = callbacks[AST_UNARY_EXPR];
    if (!cb.isNull())
        return callback(cb, opName, expr, pos, dst);

    return newNode(AST_UNARY_EXPR, pos,
                   "operator", opName,
                   "argument", expr,
                   "prefix", BooleanValue(true),
                   dst);
}

bool
NodeBuilder::assignmentExpression(AssignmentOperator aop, Value lhs, Value rhs,
                                  TokenPos *pos, Value *dst)
{
    JS_ASSERT(aop > AOP_ERR && aop < AOP_LIMIT);

    Value opName;
    if (!atomValue(aopNames[aop], &opName))
        return false;

    Value cb = callbacks[AST_ASSIGN_EXPR];
    if (!cb.isNull())
        return callback(cb, opName, lhs, rhs, pos, dst);

    return newNode(AST_ASSIGN_EXPR, pos,
                   "operator", opName,
                   "left", lhs,
                   "right", rhs,
                   dst);
}

bool
NodeBuilder::updateExpression(Value expr, bool incr, bool prefix, TokenPos *pos, Value *dst)
{
    Value opName;
    if (!atomValue(incr ? "++" : "--", &opName))
        return false;

    Value cb = callbacks[AST_UPDATE_EXPR];
    if (!cb.isNull())
        return callback(cb, expr, opName, BooleanValue(prefix), pos, dst);

    return newNode(AST_UPDATE_EXPR, pos,
                   "operator", opName,
                   "argument", expr,
                   "prefix", BooleanValue(prefix),
                   dst);
}

bool
NodeBuilder::logicalExpression(bool lor, Value left, Value right, TokenPos *pos, Value *dst)
{
    Value opName;
    if (!atomValue(lor ? "||" : "&&", &opName))
        return false;

    Value cb = callbacks[AST_LOGICAL_EXPR];
    if (!cb.isNull())
        return callback(cb, opName, left, right, pos, dst);

    return newNode(AST_LOGICAL_EXPR, pos,
                   "operator", opName,
                   "left", left,
                   "right", right,
                   dst);
}

bool
NodeBuilder::conditionalExpression(Value test, Value cons, Value alt, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_COND_EXPR];
    if (!cb.isNull())
        return callback(cb, test, cons, alt, pos, dst);

    return newNode(AST_COND_EXPR, pos,
                   "test", test,
                   "consequent", cons,
                   "alternate", alt,
                   dst);
}

bool
NodeBuilder::sequenceExpression(NodeVector &elts, TokenPos *pos, Value *dst)
{
    return listNode(AST_LIST_EXPR, "expressions", elts, pos, dst);
}

bool
NodeBuilder::callExpression(Value callee, NodeVector &args, TokenPos *pos, Value *dst)
{
    Value array;
    if (!newArray(args, &array))
        return false;

    Value cb = callbacks[AST_CALL_EXPR];
    if (!cb.isNull())
        return callback(cb, callee, array, pos, dst);

    return newNode(AST_CALL_EXPR, pos,
                   "callee", callee,
                   "arguments", array,
                   dst);
}

bool
NodeBuilder::newExpression(Value callee, NodeVector &args, TokenPos *pos, Value *dst)
{
    Value array;
    if (!newArray(args, &array))
        return false;

    Value cb = callbacks[AST_NEW_EXPR];
    if (!cb.isNull())
        return callback(cb, callee, array, pos, dst);

    return newNode(AST_NEW_EXPR, pos,
                   "callee", callee,
                   "arguments", array,
                   dst);
}

bool
NodeBuilder::memberExpression(bool computed, Value expr, Value member, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_MEMBER_EXPR];
    if (!cb.isNull())
        return callback(cb, BooleanValue(computed), expr, member, pos, dst);

    return newNode(AST_MEMBER_EXPR, pos,
                   "object", expr,
                   "property", member,
                   "computed", BooleanValue(computed),
                   dst);
}

bool
NodeBuilder::arrayExpression(NodeVector &elts, TokenPos *pos, Value *dst)
{
    return listNode(AST_ARRAY_EXPR, "elements", elts, pos, dst);
}

bool
NodeBuilder::propertyPattern(Value key, Value patt, TokenPos *pos, Value *dst)
{
    Value kindName;
    if (!atomValue("init", &kindName))
        return false;

    Value cb = callbacks[AST_PROP_PATT];
    if (!cb.isNull())
        return callback(cb, key, patt, pos, dst);

    return newNode(AST_PROP_PATT, pos,
                   "key", key,
                   "value", patt,
                   "kind", kindName,
                   dst);
}

bool
NodeBuilder::propertyInitializer(Value key, Value val, PropKind kind, TokenPos *pos, Value *dst)
{
    Value kindName;
    if (!atomValue(kind == PROP_INIT
                   ? "init"
                   : kind == PROP_GETTER
                   ? "get"
                   : "set", &kindName)) {
        return false;
    }

    Value cb = callbacks[AST_PROPERTY];
    if (!cb.isNull())
        return callback(cb, kindName, key, val, pos, dst);

    return newNode(AST_PROPERTY, pos,
                   "key", key,
                   "value", val,
                   "kind", kindName,
                   dst);
}

bool
NodeBuilder::objectExpression(NodeVector &elts, TokenPos *pos, Value *dst)
{
    return listNode(AST_OBJECT_EXPR, "properties", elts, pos, dst);
}

bool
NodeBuilder::thisExpression(TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_THIS_EXPR];
    if (!cb.isNull())
        return callback(cb, pos, dst);

    return newNode(AST_THIS_EXPR, pos, dst);
}

bool
NodeBuilder::yieldExpression(Value arg, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_YIELD_EXPR];
    if (!cb.isNull())
        return callback(cb, opt(arg), pos, dst);

    return newNode(AST_YIELD_EXPR, pos, "argument", arg, dst);
}

bool
NodeBuilder::comprehensionBlock(Value patt, Value src, bool isForEach, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_COMP_BLOCK];
    if (!cb.isNull())
        return callback(cb, patt, src, BooleanValue(isForEach), pos, dst);

    return newNode(AST_COMP_BLOCK, pos,
                   "left", patt,
                   "right", src,
                   "each", BooleanValue(isForEach),
                   dst);
}

bool
NodeBuilder::comprehensionExpression(Value body, NodeVector &blocks, Value filter,
                                     TokenPos *pos, Value *dst)
{
    Value array;
    if (!newArray(blocks, &array))
        return false;

    Value cb = callbacks[AST_COMP_EXPR];
    if (!cb.isNull())
        return callback(cb, body, array, opt(filter), pos, dst);

    return newNode(AST_COMP_EXPR, pos,
                   "body", body,
                   "blocks", array,
                   "filter", filter,
                   dst);
}

bool
NodeBuilder::generatorExpression(Value body, NodeVector &blocks, Value filter, TokenPos *pos, Value *dst)
{
    Value array;
    if (!newArray(blocks, &array))
        return false;

    Value cb = callbacks[AST_GENERATOR_EXPR];
    if (!cb.isNull())
        return callback(cb, body, array, opt(filter), pos, dst);

    return newNode(AST_GENERATOR_EXPR, pos,
                   "body", body,
                   "blocks", array,
                   "filter", filter,
                   dst);
}

bool
NodeBuilder::graphExpression(jsint idx, Value expr, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_GRAPH_EXPR];
    if (!cb.isNull())
        return callback(cb, NumberValue(idx), pos, dst);

    return newNode(AST_GRAPH_EXPR, pos,
                   "index", NumberValue(idx),
                   "expression", expr,
                   dst);
}

bool
NodeBuilder::graphIndexExpression(jsint idx, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_GRAPH_IDX_EXPR];
    if (!cb.isNull())
        return callback(cb, NumberValue(idx), pos, dst);

    return newNode(AST_GRAPH_IDX_EXPR, pos, "index", NumberValue(idx), dst);
}

bool
NodeBuilder::letExpression(NodeVector &head, Value expr, TokenPos *pos, Value *dst)
{
    Value array;
    if (!newArray(head, &array))
        return false;

    Value cb = callbacks[AST_LET_EXPR];
    if (!cb.isNull())
        return callback(cb, array, expr, pos, dst);

    return newNode(AST_LET_EXPR, pos,
                   "head", array,
                   "body", expr,
                   dst);
}

bool
NodeBuilder::letStatement(NodeVector &head, Value stmt, TokenPos *pos, Value *dst)
{
    Value array;
    if (!newArray(head, &array))
        return false;

    Value cb = callbacks[AST_LET_STMT];
    if (!cb.isNull())
        return callback(cb, array, stmt, pos, dst);

    return newNode(AST_LET_STMT, pos,
                   "head", array,
                   "body", stmt,
                   dst);
}

bool
NodeBuilder::variableDeclaration(NodeVector &elts, VarDeclKind kind, TokenPos *pos, Value *dst)
{
    JS_ASSERT(kind > VARDECL_ERR && kind < VARDECL_LIMIT);

    Value array, kindName;
    if (!newArray(elts, &array) ||
        !atomValue(kind == VARDECL_CONST
                   ? "const"
                   : kind == VARDECL_LET
                   ? "let"
                   : "var", &kindName)) {
        return false;
    }

    Value cb = callbacks[AST_VAR_DECL];
    if (!cb.isNull())
        return callback(cb, kindName, array, pos, dst);

    return newNode(AST_VAR_DECL, pos,
                   "kind", kindName,
                   "declarations", array,
                   dst);
}

bool
NodeBuilder::variableDeclarator(Value id, Value init, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_VAR_DTOR];
    if (!cb.isNull())
        return callback(cb, id, opt(init), pos, dst);

    return newNode(AST_VAR_DTOR, pos, "id", id, "init", init, dst);
}

bool
NodeBuilder::switchCase(Value expr, NodeVector &elts, TokenPos *pos, Value *dst)
{
    Value array;
    if (!newArray(elts, &array))
        return false;

    Value cb = callbacks[AST_CASE];
    if (!cb.isNull())
        return callback(cb, opt(expr), array, pos, dst);

    return newNode(AST_CASE, pos,
                   "test", expr,
                   "consequent", array,
                   dst);
}

bool
NodeBuilder::catchClause(Value var, Value guard, Value body, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_CATCH];
    if (!cb.isNull())
        return callback(cb, var, opt(guard), body, pos, dst);

    return newNode(AST_CATCH, pos,
                   "param", var,
                   "guard", guard,
                   "body", body,
                   dst);
}

bool
NodeBuilder::literal(Value val, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_LITERAL];
    if (!cb.isNull())
        return callback(cb, val, pos, dst);

    return newNode(AST_LITERAL, pos, "value", val, dst);
}

bool
NodeBuilder::identifier(Value name, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_IDENTIFIER];
    if (!cb.isNull())
        return callback(cb, name, pos, dst);

    return newNode(AST_IDENTIFIER, pos, "name", name, dst);
}

bool
NodeBuilder::objectPattern(NodeVector &elts, TokenPos *pos, Value *dst)
{
    return listNode(AST_OBJECT_PATT, "properties", elts, pos, dst);
}

bool
NodeBuilder::arrayPattern(NodeVector &elts, TokenPos *pos, Value *dst)
{
    return listNode(AST_ARRAY_PATT, "elements", elts, pos, dst);
}

bool
NodeBuilder::function(ASTType type, TokenPos *pos,
                      Value id, NodeVector &args, Value body,
                      bool isGenerator, bool isExpression,
                      Value *dst)
{
    Value array;
    if (!newArray(args, &array))
        return false;

    Value cb = callbacks[type];
    if (!cb.isNull()) {
        return callback(cb, opt(id), array, body, BooleanValue(isGenerator),
                        BooleanValue(isExpression), pos, dst);
    }

    return newNode(type, pos,
                   "id", id,
                   "params", array,
                   "body", body,
                   "generator", BooleanValue(isGenerator),
                   "expression", BooleanValue(isExpression),
                   dst);
}

bool
NodeBuilder::xmlAnyName(TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_XMLANYNAME];
    if (!cb.isNull())
        return callback(cb, pos, dst);

    return newNode(AST_XMLANYNAME, pos, dst);
}

bool
NodeBuilder::xmlEscapeExpression(Value expr, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_XMLESCAPE];
    if (!cb.isNull())
        return callback(cb, expr, pos, dst);

    return newNode(AST_XMLESCAPE, pos, "expression", expr, dst);
}

bool
NodeBuilder::xmlFilterExpression(Value left, Value right, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_XMLFILTER];
    if (!cb.isNull())
        return callback(cb, left, right, pos, dst);

    return newNode(AST_XMLFILTER, pos, "left", left, "right", right, dst);
}

bool
NodeBuilder::xmlDefaultNamespace(Value ns, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_XMLDEFAULT];
    if (!cb.isNull())
        return callback(cb, ns, pos, dst);

    return newNode(AST_XMLDEFAULT, pos, "namespace", ns, dst);
}

bool
NodeBuilder::xmlAttributeSelector(Value expr, bool computed, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_XMLATTR_SEL];
    if (!cb.isNull())
        return callback(cb, expr, BooleanValue(computed), pos, dst);

    return newNode(AST_XMLATTR_SEL, pos,
                   "attribute", expr,
                   "computed", BooleanValue(computed),
                   dst);
}

bool
NodeBuilder::xmlFunctionQualifiedIdentifier(Value right, bool computed, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_XMLFUNCQUAL];
    if (!cb.isNull())
        return callback(cb, right, BooleanValue(computed), pos, dst);

    return newNode(AST_XMLFUNCQUAL, pos,
                   "right", right,
                   "computed", BooleanValue(computed),
                   dst);
}

bool
NodeBuilder::xmlQualifiedIdentifier(Value left, Value right, bool computed,
                                    TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_XMLQUAL];
    if (!cb.isNull())
        return callback(cb, left, right, BooleanValue(computed), pos, dst);

    return newNode(AST_XMLQUAL, pos,
                   "left", left,
                   "right", right,
                   "computed", BooleanValue(computed),
                   dst);
}

bool
NodeBuilder::xmlElement(NodeVector &elts, TokenPos *pos, Value *dst)
{
    return listNode(AST_XMLELEM, "contents", elts, pos, dst);
}

bool
NodeBuilder::xmlText(Value text, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_XMLTEXT];
    if (!cb.isNull())
        return callback(cb, text, pos, dst);

    return newNode(AST_XMLTEXT, pos, "text", text, dst);
}

bool
NodeBuilder::xmlList(NodeVector &elts, TokenPos *pos, Value *dst)
{
    return listNode(AST_XMLLIST, "contents", elts, pos, dst);
}

bool
NodeBuilder::xmlStartTag(NodeVector &elts, TokenPos *pos, Value *dst)
{
    return listNode(AST_XMLSTART, "contents", elts, pos, dst);
}

bool
NodeBuilder::xmlEndTag(NodeVector &elts, TokenPos *pos, Value *dst)
{
    return listNode(AST_XMLEND, "contents", elts, pos, dst);
}

bool
NodeBuilder::xmlPointTag(NodeVector &elts, TokenPos *pos, Value *dst)
{
    return listNode(AST_XMLPOINT, "contents", elts, pos, dst);
}

bool
NodeBuilder::xmlName(Value text, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_XMLNAME];
    if (!cb.isNull())
        return callback(cb, text, pos, dst);

    return newNode(AST_XMLNAME, pos, "contents", text, dst);
}

bool
NodeBuilder::xmlName(NodeVector &elts, TokenPos *pos, Value *dst)
{
    return listNode(AST_XMLNAME, "contents", elts, pos ,dst);
}

bool
NodeBuilder::xmlAttribute(Value text, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_XMLATTR];
    if (!cb.isNull())
        return callback(cb, text, pos, dst);

    return newNode(AST_XMLATTR, pos, "value", text, dst);
}

bool
NodeBuilder::xmlCdata(Value text, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_XMLCDATA];
    if (!cb.isNull())
        return callback(cb, text, pos, dst);

    return newNode(AST_XMLCDATA, pos, "contents", text, dst);
}

bool
NodeBuilder::xmlComment(Value text, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_XMLCOMMENT];
    if (!cb.isNull())
        return callback(cb, text, pos, dst);

    return newNode(AST_XMLCOMMENT, pos, "contents", text, dst);
}

bool
NodeBuilder::xmlPI(Value target, TokenPos *pos, Value *dst)
{
    return xmlPI(target, NullValue(), pos, dst);
}

bool
NodeBuilder::xmlPI(Value target, Value contents, TokenPos *pos, Value *dst)
{
    Value cb = callbacks[AST_XMLPI];
    if (!cb.isNull())
        return callback(cb, target, contents, pos, dst);

    return newNode(AST_XMLPI, pos,
                   "target", target,
                   "contents", contents,
                   dst);
}


/*
 * Serialization of parse nodes to JavaScript objects.
 *
 * All serialization methods take a non-nullable JSParseNode pointer.
 */

class ASTSerializer
{
    JSContext     *cx;
    NodeBuilder   builder;
    uint32        lineno;

    Value atomContents(JSAtom *atom) {
        return StringValue(atom ? atom : cx->runtime->atomState.emptyAtom);
    }

    BinaryOperator binop(TokenKind tk, JSOp op);
    UnaryOperator unop(TokenKind tk, JSOp op);
    AssignmentOperator aop(JSOp op);

    bool statements(JSParseNode *pn, NodeVector &elts);
    bool expressions(JSParseNode *pn, NodeVector &elts);
    bool xmls(JSParseNode *pn, NodeVector &elts);
    bool leftAssociate(JSParseNode *pn, Value *dst);
    bool functionArgs(JSParseNode *pn, JSParseNode *pnargs, JSParseNode *pndestruct,
                      JSParseNode *pnbody, NodeVector &args);

    bool sourceElement(JSParseNode *pn, Value *dst);

    bool declaration(JSParseNode *pn, Value *dst);
    bool variableDeclaration(JSParseNode *pn, bool let, Value *dst);
    bool variableDeclarator(JSParseNode *pn, VarDeclKind *pkind, Value *dst);
    bool letHead(JSParseNode *pn, NodeVector &dtors);

    bool optStatement(JSParseNode *pn, Value *dst) {
        if (!pn) {
            dst->setMagic(JS_SERIALIZE_NO_NODE);
            return true;
        }
        return statement(pn, dst);
    }

    bool forInit(JSParseNode *pn, Value *dst);
    bool statement(JSParseNode *pn, Value *dst);
    bool blockStatement(JSParseNode *pn, Value *dst);
    bool switchStatement(JSParseNode *pn, Value *dst);
    bool switchCase(JSParseNode *pn, Value *dst);
    bool tryStatement(JSParseNode *pn, Value *dst);
    bool catchClause(JSParseNode *pn, Value *dst);

    bool optExpression(JSParseNode *pn, Value *dst) {
        if (!pn) {
            dst->setMagic(JS_SERIALIZE_NO_NODE);
            return true;
        }
        return expression(pn, dst);
    }

    bool expression(JSParseNode *pn, Value *dst);

    bool propertyName(JSParseNode *pn, Value *dst);
    bool property(JSParseNode *pn, Value *dst);

    bool optIdentifier(JSAtom *atom, TokenPos *pos, Value *dst) {
        if (!atom) {
            dst->setMagic(JS_SERIALIZE_NO_NODE);
            return true;
        }
        return identifier(atom, pos, dst);
    }

    bool identifier(JSAtom *atom, TokenPos *pos, Value *dst);
    bool identifier(JSParseNode *pn, Value *dst);
    bool literal(JSParseNode *pn, Value *dst);

    bool pattern(JSParseNode *pn, VarDeclKind *pkind, Value *dst);
    bool arrayPattern(JSParseNode *pn, VarDeclKind *pkind, Value *dst);
    bool objectPattern(JSParseNode *pn, VarDeclKind *pkind, Value *dst);

    bool function(JSParseNode *pn, ASTType type, Value *dst);
    bool functionArgsAndBody(JSParseNode *pn, NodeVector &args, Value *body);
    bool functionBody(JSParseNode *pn, TokenPos *pos, Value *dst);

    bool comprehensionBlock(JSParseNode *pn, Value *dst);
    bool comprehension(JSParseNode *pn, Value *dst);
    bool generatorExpression(JSParseNode *pn, Value *dst);

    bool xml(JSParseNode *pn, Value *dst);

  public:
    ASTSerializer(JSContext *c, bool l, char const *src, uint32 ln)
        : cx(c), builder(c, l, src), lineno(ln) {
    }

    bool init(JSObject *userobj) {
        return builder.init(userobj);
    }

    bool program(JSParseNode *pn, Value *dst);
};

AssignmentOperator
ASTSerializer::aop(JSOp op)
{
    switch (op) {
      case JSOP_NOP:
        return AOP_ASSIGN;
      case JSOP_ADD:
        return AOP_PLUS;
      case JSOP_SUB:
        return AOP_MINUS;
      case JSOP_MUL:
        return AOP_STAR;
      case JSOP_DIV:
        return AOP_DIV;
      case JSOP_MOD:
        return AOP_MOD;
      case JSOP_LSH:
        return AOP_LSH;
      case JSOP_RSH:
        return AOP_RSH;
      case JSOP_URSH:
        return AOP_URSH;
      case JSOP_BITOR:
        return AOP_BITOR;
      case JSOP_BITXOR:
        return AOP_BITXOR;
      case JSOP_BITAND:
        return AOP_BITAND;
      default:
        return AOP_ERR;
    }
}

UnaryOperator
ASTSerializer::unop(TokenKind tk, JSOp op)
{
    if (tk == TOK_DELETE)
        return UNOP_DELETE;

    switch (op) {
      case JSOP_NEG:
        return UNOP_NEG;
      case JSOP_POS:
        return UNOP_POS;
      case JSOP_NOT:
        return UNOP_NOT;
      case JSOP_BITNOT:
        return UNOP_BITNOT;
      case JSOP_TYPEOF:
      case JSOP_TYPEOFEXPR:
        return UNOP_TYPEOF;
      case JSOP_VOID:
        return UNOP_VOID;
      default:
        return UNOP_ERR;
    }
}

BinaryOperator
ASTSerializer::binop(TokenKind tk, JSOp op)
{
    switch (tk) {
      case TOK_EQOP:
        switch (op) {
          case JSOP_EQ:
            return BINOP_EQ;
          case JSOP_NE:
            return BINOP_NE;
          case JSOP_STRICTEQ:
            return BINOP_STRICTEQ;
          case JSOP_STRICTNE:
            return BINOP_STRICTNE;
          default:
            return BINOP_ERR;
        }

      case TOK_RELOP:
        switch (op) {
          case JSOP_LT:
            return BINOP_LT;
          case JSOP_LE:
            return BINOP_LE;
          case JSOP_GT:
            return BINOP_GT;
          case JSOP_GE:
            return BINOP_GE;
          default:
            return BINOP_ERR;
        }

      case TOK_SHOP:
        switch (op) {
          case JSOP_LSH:
            return BINOP_LSH;
          case JSOP_RSH:
            return BINOP_RSH;
          case JSOP_URSH:
            return BINOP_URSH;
          default:
            return BINOP_ERR;
        }

      case TOK_PLUS:
        return BINOP_PLUS;
      case TOK_MINUS:
        return BINOP_MINUS;
      case TOK_STAR:
        return BINOP_STAR;
      case TOK_DIVOP:
        return (op == JSOP_MOD) ? BINOP_MOD : BINOP_DIV;
      case TOK_BITOR:
        return BINOP_BITOR;
      case TOK_BITXOR:
        return BINOP_BITXOR;
      case TOK_BITAND:
        return BINOP_BITAND;
      case TOK_IN:
        return BINOP_IN;
      case TOK_INSTANCEOF:
        return BINOP_INSTANCEOF;
      case TOK_DBLDOT:
        return BINOP_DBLDOT;
      default:
        return BINOP_ERR;
    }
}

bool
ASTSerializer::statements(JSParseNode *pn, NodeVector &elts)
{
    JS_ASSERT(PN_TYPE(pn) == TOK_LC && pn->pn_arity == PN_LIST);

    if (!elts.reserve(pn->pn_count))
        return false;

    for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
        Value elt;
        if (!sourceElement(next, &elt))
            return false;
        elts.infallibleAppend(elt);
    }

    return true;
}

bool
ASTSerializer::expressions(JSParseNode *pn, NodeVector &elts)
{
    if (!elts.reserve(pn->pn_count))
        return false;

    for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
        Value elt;
        if (!expression(next, &elt))
            return false;
        elts.infallibleAppend(elt);
    }

    return true;
}

bool
ASTSerializer::xmls(JSParseNode *pn, NodeVector &elts)
{
    if (!elts.reserve(pn->pn_count))
        return false;

    for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
        Value elt;
        if (!xml(next, &elt))
            return false;
        elts.infallibleAppend(elt);
    }

    return true;
}

bool
ASTSerializer::blockStatement(JSParseNode *pn, Value *dst)
{
    JS_ASSERT(PN_TYPE(pn) == TOK_LC);

    NodeVector stmts(cx);
    return statements(pn, stmts) &&
           builder.blockStatement(stmts, &pn->pn_pos, dst);
}

bool
ASTSerializer::program(JSParseNode *pn, Value *dst)
{
    JS_ASSERT(pn->pn_pos.begin.lineno == lineno);

    NodeVector stmts(cx);
    return statements(pn, stmts) &&
           builder.program(stmts, &pn->pn_pos, dst);
}

bool
ASTSerializer::sourceElement(JSParseNode *pn, Value *dst)
{
    /* SpiderMonkey allows declarations even in pure statement contexts. */
    return statement(pn, dst);
}

bool
ASTSerializer::declaration(JSParseNode *pn, Value *dst)
{
    JS_ASSERT(PN_TYPE(pn) == TOK_FUNCTION ||
              PN_TYPE(pn) == TOK_VAR ||
              PN_TYPE(pn) == TOK_LET);

    switch (PN_TYPE(pn)) {
      case TOK_FUNCTION:
        return function(pn, AST_FUNC_DECL, dst);

      case TOK_VAR:
        return variableDeclaration(pn, false, dst);

      default:
        JS_ASSERT(PN_TYPE(pn) == TOK_LET);
        return variableDeclaration(pn, true, dst);
    }
}

bool
ASTSerializer::variableDeclaration(JSParseNode *pn, bool let, Value *dst)
{
    JS_ASSERT(let ? PN_TYPE(pn) == TOK_LET : PN_TYPE(pn) == TOK_VAR);

    /* Later updated to VARDECL_CONST if we find a PND_CONST declarator. */
    VarDeclKind kind = let ? VARDECL_LET : VARDECL_VAR;

    NodeVector dtors(cx);

    /* In a for-in context, variable declarations contain just a single pattern. */
    if (pn->pn_xflags & PNX_FORINVAR) {
        Value patt, child;
        return pattern(pn->pn_head, &kind, &patt) &&
               builder.variableDeclarator(patt, NullValue(), &pn->pn_head->pn_pos, &child) &&
               dtors.append(child) &&
               builder.variableDeclaration(dtors, kind, &pn->pn_pos, dst);
    }

    if (!dtors.reserve(pn->pn_count))
        return false;
    for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
        Value child;
        if (!variableDeclarator(next, &kind, &child))
            return false;
        dtors.infallibleAppend(child);
    }

    return builder.variableDeclaration(dtors, kind, &pn->pn_pos, dst);
}

bool
ASTSerializer::variableDeclarator(JSParseNode *pn, VarDeclKind *pkind, Value *dst)
{
    /* A destructuring declarator is always a TOK_ASSIGN. */
    JS_ASSERT(PN_TYPE(pn) == TOK_NAME || PN_TYPE(pn) == TOK_ASSIGN);

    JSParseNode *pnleft;
    JSParseNode *pnright;

    if (PN_TYPE(pn) == TOK_NAME) {
        pnleft = pn;
        pnright = pn->pn_used ? NULL : pn->pn_expr;
    } else {
        JS_ASSERT(PN_TYPE(pn) == TOK_ASSIGN);
        pnleft = pn->pn_left;
        pnright = pn->pn_right;
    }

    Value left, right;
    return pattern(pnleft, pkind, &left) &&
           optExpression(pnright, &right) &&
           builder.variableDeclarator(left, right, &pn->pn_pos, dst);
}

bool
ASTSerializer::letHead(JSParseNode *pn, NodeVector &dtors)
{
    if (!dtors.reserve(pn->pn_count))
        return false;

    VarDeclKind kind = VARDECL_LET_HEAD;

    for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
        Value child;
        /*
         * Unlike in |variableDeclaration|, this does not update |kind|; since let-heads do
         * not contain const declarations, declarators should never have PND_CONST set.
         */
        if (!variableDeclarator(next, &kind, &child))
            return false;
        dtors.infallibleAppend(child);
    }

    return true;
}

bool
ASTSerializer::switchCase(JSParseNode *pn, Value *dst)
{
    NodeVector stmts(cx);

    Value expr;

    return optExpression(pn->pn_left, &expr) &&
           statements(pn->pn_right, stmts) &&
           builder.switchCase(expr, stmts, &pn->pn_pos, dst);
}

bool
ASTSerializer::switchStatement(JSParseNode *pn, Value *dst)
{
    Value disc;

    if (!expression(pn->pn_left, &disc))
        return false;

    JSParseNode *listNode;
    bool lexical;

    if (PN_TYPE(pn->pn_right) == TOK_LEXICALSCOPE) {
        listNode = pn->pn_right->pn_expr;
        lexical = true;
    } else {
        listNode = pn->pn_right;
        lexical = false;
    }

    NodeVector cases(cx);
    if (!cases.reserve(listNode->pn_count))
        return false;

    for (JSParseNode *next = listNode->pn_head; next; next = next->pn_next) {
        Value child;
#ifdef __GNUC__ /* quell GCC overwarning */
        child = UndefinedValue();
#endif
        if (!switchCase(next, &child))
            return false;
        cases.infallibleAppend(child);
    }

    return builder.switchStatement(disc, cases, lexical, &pn->pn_pos, dst);
}

bool
ASTSerializer::catchClause(JSParseNode *pn, Value *dst)
{
    Value var, guard, body;

    return pattern(pn->pn_kid1, NULL, &var) &&
           optExpression(pn->pn_kid2, &guard) &&
           statement(pn->pn_kid3, &body) &&
           builder.catchClause(var, guard, body, &pn->pn_pos, dst);
}

bool
ASTSerializer::tryStatement(JSParseNode *pn, Value *dst)
{
    Value body;
    if (!statement(pn->pn_kid1, &body))
        return false;

    NodeVector clauses(cx);
    if (pn->pn_kid2) {
        if (!clauses.reserve(pn->pn_kid2->pn_count))
            return false;

        for (JSParseNode *next = pn->pn_kid2->pn_head; next; next = next->pn_next) {
            Value clause;
            if (!catchClause(next->pn_expr, &clause))
                return false;
            clauses.infallibleAppend(clause);
        }
    }

    Value finally;
    return optStatement(pn->pn_kid3, &finally) &&
           builder.tryStatement(body, clauses, finally, &pn->pn_pos, dst);
}

bool
ASTSerializer::forInit(JSParseNode *pn, Value *dst)
{
    if (!pn) {
        dst->setMagic(JS_SERIALIZE_NO_NODE);
        return true;
    }

    return (PN_TYPE(pn) == TOK_VAR)
           ? variableDeclaration(pn, false, dst)
           : (PN_TYPE(pn) == TOK_LET)
           ? variableDeclaration(pn, true, dst)
           : expression(pn, dst);
}

bool
ASTSerializer::statement(JSParseNode *pn, Value *dst)
{
    JS_CHECK_RECURSION(cx, return false);
    switch (PN_TYPE(pn)) {
      case TOK_FUNCTION:
      case TOK_VAR:
      case TOK_LET:
        return declaration(pn, dst);

      case TOK_NAME:
        LOCAL_ASSERT(pn->pn_used);
        return statement(pn->pn_lexdef, dst);

      case TOK_SEMI:
        if (pn->pn_kid) {
            Value expr;
            return expression(pn->pn_kid, &expr) &&
                   builder.expressionStatement(expr, &pn->pn_pos, dst);
        }
        return builder.emptyStatement(&pn->pn_pos, dst);

      case TOK_LEXICALSCOPE:
        pn = pn->pn_expr;
        if (PN_TYPE(pn) == TOK_LET) {
            NodeVector dtors(cx);
            Value stmt;

            return letHead(pn->pn_left, dtors) &&
                   statement(pn->pn_right, &stmt) &&
                   builder.letStatement(dtors, stmt, &pn->pn_pos, dst);
        }

        if (PN_TYPE(pn) != TOK_LC)
            return statement(pn, dst);
        /* FALL THROUGH */

      case TOK_LC:
        return blockStatement(pn, dst);

      case TOK_IF:
      {
        Value test, cons, alt;

        return expression(pn->pn_kid1, &test) &&
               statement(pn->pn_kid2, &cons) &&
               optStatement(pn->pn_kid3, &alt) &&
               builder.ifStatement(test, cons, alt, &pn->pn_pos, dst);
      }

      case TOK_SWITCH:
        return switchStatement(pn, dst);

      case TOK_TRY:
        return tryStatement(pn, dst);

      case TOK_WITH:
      case TOK_WHILE:
      {
        Value expr, stmt;

        return expression(pn->pn_left, &expr) &&
               statement(pn->pn_right, &stmt) &&
               (PN_TYPE(pn) == TOK_WITH)
               ? builder.withStatement(expr, stmt, &pn->pn_pos, dst)
               : builder.whileStatement(expr, stmt, &pn->pn_pos, dst);
      }

      case TOK_DO:
      {
        Value stmt, test;

        return statement(pn->pn_left, &stmt) &&
               expression(pn->pn_right, &test) &&
               builder.doWhileStatement(stmt, test, &pn->pn_pos, dst);
      }

      case TOK_FOR:
      {
        JSParseNode *head = pn->pn_left;

        Value stmt;
        if (!statement(pn->pn_right, &stmt))
            return false;

        bool isForEach = pn->pn_iflags & JSITER_FOREACH;

        if (PN_TYPE(head) == TOK_IN) {
            Value var, expr;

            return (!head->pn_kid1
                    ? pattern(head->pn_kid2, NULL, &var)
                    : variableDeclaration(head->pn_kid1,
                                          PN_TYPE(head->pn_kid1) == TOK_LET,
                                          &var)) &&
                   expression(head->pn_kid3, &expr) &&
                   builder.forInStatement(var, expr, stmt, isForEach, &pn->pn_pos, dst);
        }

        Value init, test, update;

        return forInit(head->pn_kid1, &init) &&
               optExpression(head->pn_kid2, &test) &&
               optExpression(head->pn_kid3, &update) &&
               builder.forStatement(init, test, update, stmt, &pn->pn_pos, dst);
      }

      /* Synthesized by the parser when a for-in loop contains a variable initializer. */
      case TOK_SEQ:
      {
        LOCAL_ASSERT(pn->pn_count == 2);

        JSParseNode *prelude = pn->pn_head;
        JSParseNode *loop = prelude->pn_next;

        LOCAL_ASSERT(PN_TYPE(prelude) == TOK_VAR && PN_TYPE(loop) == TOK_FOR);

        Value var;
        if (!variableDeclaration(prelude, false, &var))
            return false;

        JSParseNode *head = loop->pn_left;
        JS_ASSERT(PN_TYPE(head) == TOK_IN);

        bool isForEach = loop->pn_iflags & JSITER_FOREACH;

        Value expr, stmt;

        return expression(head->pn_kid3, &expr) &&
               statement(loop->pn_right, &stmt) &&
               builder.forInStatement(var, expr, stmt, isForEach, &pn->pn_pos, dst);
      }

      case TOK_BREAK:
      case TOK_CONTINUE:
      {
        Value label;

        return optIdentifier(pn->pn_atom, NULL, &label) &&
               (PN_TYPE(pn) == TOK_BREAK
                ? builder.breakStatement(label, &pn->pn_pos, dst)
                : builder.continueStatement(label, &pn->pn_pos, dst));
      }

      case TOK_COLON:
      {
        Value label, stmt;

        return identifier(pn->pn_atom, NULL, &label) &&
               statement(pn->pn_expr, &stmt) &&
               builder.labeledStatement(label, stmt, &pn->pn_pos, dst);
      }

      case TOK_THROW:
      case TOK_RETURN:
      {
        Value arg;

        return optExpression(pn->pn_kid, &arg) &&
               (PN_TYPE(pn) == TOK_THROW
                ? builder.throwStatement(arg, &pn->pn_pos, dst)
                : builder.returnStatement(arg, &pn->pn_pos, dst));
      }

      case TOK_DEBUGGER:
        return builder.debuggerStatement(&pn->pn_pos, dst);

#if JS_HAS_XML_SUPPORT
      case TOK_DEFAULT:
      {
        LOCAL_ASSERT(pn->pn_arity == PN_UNARY);

        Value ns;

        return expression(pn->pn_kid, &ns) &&
               builder.xmlDefaultNamespace(ns, &pn->pn_pos, dst);
      }
#endif

      default:
        LOCAL_NOT_REACHED("unexpected statement type");
    }
}

bool
ASTSerializer::leftAssociate(JSParseNode *pn, Value *dst)
{
    JS_ASSERT(pn->pn_arity == PN_LIST);
    JS_ASSERT(pn->pn_count >= 1);

    TokenKind tk = PN_TYPE(pn);
    bool lor = tk == TOK_OR;
    bool logop = lor || (tk == TOK_AND);

    JSParseNode *head = pn->pn_head;
    Value left;
    if (!expression(head, &left))
        return false;
    for (JSParseNode *next = head->pn_next; next; next = next->pn_next) {
        Value right;
        if (!expression(next, &right))
            return false;

        TokenPos subpos = { pn->pn_pos.begin, next->pn_pos.end };

        if (logop) {
            if (!builder.logicalExpression(lor, left, right, &subpos, &left))
                return false;
        } else {
            BinaryOperator op = binop(PN_TYPE(pn), PN_OP(pn));
            LOCAL_ASSERT(op > BINOP_ERR && op < BINOP_LIMIT);

            if (!builder.binaryExpression(op, left, right, &subpos, &left))
                return false;
        }
    }

    *dst = left;
    return true;
}

bool
ASTSerializer::comprehensionBlock(JSParseNode *pn, Value *dst)
{
    LOCAL_ASSERT(pn->pn_arity == PN_BINARY);

    JSParseNode *in = pn->pn_left;

    LOCAL_ASSERT(in && PN_TYPE(in) == TOK_IN);

    bool isForEach = pn->pn_iflags & JSITER_FOREACH;

    Value patt, src;
    return pattern(in->pn_kid2, NULL, &patt) &&
           expression(in->pn_kid3, &src) &&
           builder.comprehensionBlock(patt, src, isForEach, &in->pn_pos, dst);
}

bool
ASTSerializer::comprehension(JSParseNode *pn, Value *dst)
{
    LOCAL_ASSERT(PN_TYPE(pn) == TOK_FOR);

    NodeVector blocks(cx);

    JSParseNode *next = pn;
    while (PN_TYPE(next) == TOK_FOR) {
        Value block;
        if (!comprehensionBlock(next, &block) || !blocks.append(block))
            return false;
        next = next->pn_right;
    }

    Value filter = MagicValue(JS_SERIALIZE_NO_NODE);

    if (PN_TYPE(next) == TOK_IF) {
        if (!optExpression(next->pn_kid1, &filter))
            return false;
        next = next->pn_kid2;
    } else if (PN_TYPE(next) == TOK_LC && next->pn_count == 0) {
        /* js_FoldConstants optimized away the push. */
        NodeVector empty(cx);
        return builder.arrayExpression(empty, &pn->pn_pos, dst);
    }

    LOCAL_ASSERT(PN_TYPE(next) == TOK_ARRAYPUSH);

    Value body;

    return expression(next->pn_kid, &body) &&
           builder.comprehensionExpression(body, blocks, filter, &pn->pn_pos, dst);
}

bool
ASTSerializer::generatorExpression(JSParseNode *pn, Value *dst)
{
    LOCAL_ASSERT(PN_TYPE(pn) == TOK_FOR);

    NodeVector blocks(cx);

    JSParseNode *next = pn;
    while (PN_TYPE(next) == TOK_FOR) {
        Value block;
        if (!comprehensionBlock(next, &block) || !blocks.append(block))
            return false;
        next = next->pn_right;
    }

    Value filter = MagicValue(JS_SERIALIZE_NO_NODE);

    if (PN_TYPE(next) == TOK_IF) {
        if (!optExpression(next->pn_kid1, &filter))
            return false;
        next = next->pn_kid2;
    }

    LOCAL_ASSERT(PN_TYPE(next) == TOK_SEMI &&
                 PN_TYPE(next->pn_kid) == TOK_YIELD &&
                 next->pn_kid->pn_kid);

    Value body;

    return expression(next->pn_kid->pn_kid, &body) &&
           builder.generatorExpression(body, blocks, filter, &pn->pn_pos, dst);
}

bool
ASTSerializer::expression(JSParseNode *pn, Value *dst)
{
    JS_CHECK_RECURSION(cx, return false);
    switch (PN_TYPE(pn)) {
      case TOK_FUNCTION:
        return function(pn, AST_FUNC_EXPR, dst);

      case TOK_COMMA:
      {
        NodeVector exprs(cx);
        return expressions(pn, exprs) &&
               builder.sequenceExpression(exprs, &pn->pn_pos, dst);
      }

      case TOK_HOOK:
      {
        Value test, cons, alt;

        return expression(pn->pn_kid1, &test) &&
               expression(pn->pn_kid2, &cons) &&
               expression(pn->pn_kid3, &alt) &&
               builder.conditionalExpression(test, cons, alt, &pn->pn_pos, dst);
      }

      case TOK_OR:
      case TOK_AND:
      {
        if (pn->pn_arity == PN_BINARY) {
            Value left, right;
            return expression(pn->pn_left, &left) &&
                   expression(pn->pn_right, &right) &&
                   builder.logicalExpression(PN_TYPE(pn) == TOK_OR, left, right, &pn->pn_pos, dst);
        }
        return leftAssociate(pn, dst);
      }

      case TOK_INC:
      case TOK_DEC:
      {
        bool incr = PN_TYPE(pn) == TOK_INC;
        bool prefix = PN_OP(pn) >= JSOP_INCNAME && PN_OP(pn) <= JSOP_DECELEM;

        Value expr;
        return expression(pn->pn_kid, &expr) &&
               builder.updateExpression(expr, incr, prefix, &pn->pn_pos, dst);
      }

      case TOK_ASSIGN:
      {
        AssignmentOperator op = aop(PN_OP(pn));
        LOCAL_ASSERT(op > AOP_ERR && op < AOP_LIMIT);

        Value lhs, rhs;
        return pattern(pn->pn_left, NULL, &lhs) &&
               expression(pn->pn_right, &rhs) &&
               builder.assignmentExpression(op, lhs, rhs, &pn->pn_pos, dst);
      }

      case TOK_EQOP:
      case TOK_RELOP:
      case TOK_SHOP:
      case TOK_PLUS:
      case TOK_MINUS:
      case TOK_STAR:
      case TOK_DIVOP:
      case TOK_BITOR:
      case TOK_BITXOR:
      case TOK_BITAND:
      case TOK_IN:
      case TOK_INSTANCEOF:
      case TOK_DBLDOT:
        if (pn->pn_arity == PN_BINARY) {
            BinaryOperator op = binop(PN_TYPE(pn), PN_OP(pn));
            LOCAL_ASSERT(op > BINOP_ERR && op < BINOP_LIMIT);

            Value left, right;
            return expression(pn->pn_left, &left) &&
                   expression(pn->pn_right, &right) &&
                   builder.binaryExpression(op, left, right, &pn->pn_pos, dst);
        }
        return leftAssociate(pn, dst);

      case TOK_DELETE:
      case TOK_UNARYOP:
#if JS_HAS_XML_SUPPORT
        if (PN_OP(pn) == JSOP_XMLNAME ||
            PN_OP(pn) == JSOP_SETXMLNAME ||
            PN_OP(pn) == JSOP_BINDXMLNAME)
            return expression(pn->pn_kid, dst);
#endif

      {
        UnaryOperator op = unop(PN_TYPE(pn), PN_OP(pn));
        LOCAL_ASSERT(op > UNOP_ERR && op < UNOP_LIMIT);

        Value expr;
        return expression(pn->pn_kid, &expr) &&
               builder.unaryExpression(op, expr, &pn->pn_pos, dst);
      }

      case TOK_NEW:
      case TOK_LP:
      {
#ifdef JS_HAS_GENERATOR_EXPRS
        if (pn->isGeneratorExpr())
            return generatorExpression(pn->generatorExpr(), dst);
#endif

        JSParseNode *next = pn->pn_head;

        Value callee;
        if (!expression(next, &callee))
            return false;

        NodeVector args(cx);
        if (!args.reserve(pn->pn_count - 1))
            return false;

        for (next = next->pn_next; next; next = next->pn_next) {
            Value arg;
            if (!expression(next, &arg))
                return false;
            args.infallibleAppend(arg);
        }

        return PN_TYPE(pn) == TOK_NEW
               ? builder.newExpression(callee, args, &pn->pn_pos, dst)
               : builder.callExpression(callee, args, &pn->pn_pos, dst);
      }

      case TOK_DOT:
      {
        Value expr, id;
        return expression(pn->pn_expr, &expr) &&
               identifier(pn->pn_atom, NULL, &id) &&
               builder.memberExpression(false, expr, id, &pn->pn_pos, dst);
      }

      case TOK_LB:
      {
        Value left, right;
        bool computed = true;
#ifdef JS_HAS_XML_SUPPORT
        computed = (PN_TYPE(pn->pn_right) != TOK_DBLCOLON &&
                    PN_TYPE(pn->pn_right) != TOK_ANYNAME &&
                    PN_TYPE(pn->pn_right) != TOK_AT);
#endif
        return expression(pn->pn_left, &left) &&
               expression(pn->pn_right, &right) &&
               builder.memberExpression(computed, left, right, &pn->pn_pos, dst);
      }

      case TOK_RB:
      {
        NodeVector elts(cx);
        if (!elts.reserve(pn->pn_count))
            return false;

        for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
            if (PN_TYPE(next) == TOK_COMMA) {
                elts.infallibleAppend(MagicValue(JS_SERIALIZE_NO_NODE));
            } else {
                Value expr;
                if (!expression(next, &expr))
                    return false;
                elts.infallibleAppend(expr);
            }
        }

        return builder.arrayExpression(elts, &pn->pn_pos, dst);
      }

      case TOK_RC:
      {
        NodeVector elts(cx);
        if (!elts.reserve(pn->pn_count))
            return false;

        for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
            Value prop;
            if (!property(next, &prop))
                return false;
            elts.infallibleAppend(prop);
        }

        return builder.objectExpression(elts, &pn->pn_pos, dst);
      }

      case TOK_NAME:
        return identifier(pn, dst);

      case TOK_STRING:
      case TOK_REGEXP:
      case TOK_NUMBER:
      case TOK_PRIMARY:
        return PN_OP(pn) == JSOP_THIS ? builder.thisExpression(&pn->pn_pos, dst) : literal(pn, dst);

      case TOK_YIELD:
      {
        Value arg;
        return optExpression(pn->pn_kid, &arg) &&
               builder.yieldExpression(arg, &pn->pn_pos, dst);
      }

      case TOK_DEFSHARP:
      {
        Value expr;
        return expression(pn->pn_kid, &expr) &&
               builder.graphExpression(pn->pn_num, expr, &pn->pn_pos, dst);
      }

      case TOK_USESHARP:
        return builder.graphIndexExpression(pn->pn_num, &pn->pn_pos, dst);

      case TOK_ARRAYCOMP:
        /* NB: it's no longer the case that pn_count could be 2. */
        LOCAL_ASSERT(pn->pn_count == 1);
        LOCAL_ASSERT(PN_TYPE(pn->pn_head) == TOK_LEXICALSCOPE);

        return comprehension(pn->pn_head->pn_expr, dst);

      case TOK_LEXICALSCOPE:
      {
        pn = pn->pn_expr;

        NodeVector dtors(cx);
        Value expr;

        return letHead(pn->pn_left, dtors) &&
               expression(pn->pn_right, &expr) &&
               builder.letExpression(dtors, expr, &pn->pn_pos, dst);
      }

#ifdef JS_HAS_XML_SUPPORT
      case TOK_ANYNAME:
        return builder.xmlAnyName(&pn->pn_pos, dst);

      case TOK_DBLCOLON:
      {
        Value right;

        LOCAL_ASSERT(pn->pn_arity == PN_NAME || pn->pn_arity == PN_BINARY);

        JSParseNode *pnleft;
        bool computed;

        if (pn->pn_arity == PN_BINARY) {
            computed = true;
            pnleft = pn->pn_left;
            if (!expression(pn->pn_right, &right))
                return false;
        } else {
            JS_ASSERT(pn->pn_arity == PN_NAME);
            computed = false;
            pnleft = pn->pn_expr;
            if (!identifier(pn->pn_atom, NULL, &right))
                return false;
        }

        if (PN_TYPE(pnleft) == TOK_FUNCTION)
            return builder.xmlFunctionQualifiedIdentifier(right, computed, &pn->pn_pos, dst);

        Value left;
        return expression(pnleft, &left) &&
               builder.xmlQualifiedIdentifier(left, right, computed, &pn->pn_pos, dst);
      }

      case TOK_AT:
      {
        Value expr;
        JSParseNode *kid = pn->pn_kid;
        bool computed = ((PN_TYPE(kid) != TOK_NAME || PN_OP(kid) != JSOP_QNAMEPART) &&
                         PN_TYPE(kid) != TOK_DBLCOLON &&
                         PN_TYPE(kid) != TOK_ANYNAME);
        return expression(kid, &expr) &&
            builder.xmlAttributeSelector(expr, computed, &pn->pn_pos, dst);
      }

      case TOK_FILTER:
      {
        Value left, right;
        return expression(pn->pn_left, &left) &&
               expression(pn->pn_right, &right) &&
               builder.xmlFilterExpression(left, right, &pn->pn_pos, dst);
      }

      default:
        return xml(pn, dst);

#else
      default:
        LOCAL_NOT_REACHED("unexpected expression type");
#endif
    }
}

bool
ASTSerializer::xml(JSParseNode *pn, Value *dst)
{
    JS_CHECK_RECURSION(cx, return false);
    switch (PN_TYPE(pn)) {
#ifdef JS_HAS_XML_SUPPORT
      case TOK_LC:
      {
        Value expr;
        return expression(pn->pn_kid, &expr) &&
               builder.xmlEscapeExpression(expr, &pn->pn_pos, dst);
      }

      case TOK_XMLELEM:
      {
        NodeVector elts(cx);
        if (!xmls(pn, elts))
            return false;
        return builder.xmlElement(elts, &pn->pn_pos, dst);
      }

      case TOK_XMLLIST:
      {
        NodeVector elts(cx);
        if (!xmls(pn, elts))
            return false;
        return builder.xmlList(elts, &pn->pn_pos, dst);
      }

      case TOK_XMLSTAGO:
      {
        NodeVector elts(cx);
        if (!xmls(pn, elts))
            return false;
        return builder.xmlStartTag(elts, &pn->pn_pos, dst);
      }

      case TOK_XMLETAGO:
      {
        NodeVector elts(cx);
        if (!xmls(pn, elts))
            return false;
        return builder.xmlEndTag(elts, &pn->pn_pos, dst);
      }

      case TOK_XMLPTAGC:
      {
        NodeVector elts(cx);
        if (!xmls(pn, elts))
            return false;
        return builder.xmlPointTag(elts, &pn->pn_pos, dst);
      }

      case TOK_XMLTEXT:
      case TOK_XMLSPACE:
        return builder.xmlText(atomContents(pn->pn_atom), &pn->pn_pos, dst);

      case TOK_XMLNAME:
        if (pn->pn_arity == PN_NULLARY)
            return builder.xmlName(atomContents(pn->pn_atom), &pn->pn_pos, dst);

        LOCAL_ASSERT(pn->pn_arity == PN_LIST);

        {
            NodeVector elts(cx);
            return xmls(pn, elts) &&
                   builder.xmlName(elts, &pn->pn_pos, dst);
        }

      case TOK_XMLATTR:
        return builder.xmlAttribute(atomContents(pn->pn_atom), &pn->pn_pos, dst);

      case TOK_XMLCDATA:
        return builder.xmlCdata(atomContents(pn->pn_atom), &pn->pn_pos, dst);

      case TOK_XMLCOMMENT:
        return builder.xmlComment(atomContents(pn->pn_atom), &pn->pn_pos, dst);

      case TOK_XMLPI:
        if (!pn->pn_atom2)
            return builder.xmlPI(atomContents(pn->pn_atom), &pn->pn_pos, dst);
        else
            return builder.xmlPI(atomContents(pn->pn_atom),
                                 atomContents(pn->pn_atom2),
                                 &pn->pn_pos,
                                 dst);
#endif

      default:
        LOCAL_NOT_REACHED("unexpected XML node type");
    }
}

bool
ASTSerializer::propertyName(JSParseNode *pn, Value *dst)
{
    if (PN_TYPE(pn) == TOK_NAME)
        return identifier(pn, dst);

    LOCAL_ASSERT(PN_TYPE(pn) == TOK_STRING || PN_TYPE(pn) == TOK_NUMBER);

    return literal(pn, dst);
}

bool
ASTSerializer::property(JSParseNode *pn, Value *dst)
{
    PropKind kind;
    switch (PN_OP(pn)) {
      case JSOP_INITPROP:
        kind = PROP_INIT;
        break;

      case JSOP_GETTER:
        kind = PROP_GETTER;
        break;

      case JSOP_SETTER:
        kind = PROP_SETTER;
        break;

      default:
        LOCAL_NOT_REACHED("unexpected object-literal property");
    }

    Value key, val;
    return propertyName(pn->pn_left, &key) &&
           expression(pn->pn_right, &val) &&
           builder.propertyInitializer(key, val, kind, &pn->pn_pos, dst);
}

bool
ASTSerializer::literal(JSParseNode *pn, Value *dst)
{
    Value val;
    switch (PN_TYPE(pn)) {
      case TOK_STRING:
        val.setString(pn->pn_atom);
        break;

      case TOK_REGEXP:
      {
        JSObject *re1 = pn->pn_objbox ? pn->pn_objbox->object : NULL;
        LOCAL_ASSERT(re1 && re1->isRegExp());

        JSObject *proto;
        if (!js_GetClassPrototype(cx, &cx->fp()->scopeChain(), JSProto_RegExp, &proto))
            return false;

        JSObject *re2 = js_CloneRegExpObject(cx, re1, proto);
        if (!re2)
            return false;

        val.setObject(*re2);
        break;
      }

      case TOK_NUMBER:
        val.setNumber(pn->pn_dval);
        break;

      case TOK_PRIMARY:
        if (PN_OP(pn) == JSOP_NULL)
            val.setNull();
        else
            val.setBoolean(PN_OP(pn) == JSOP_TRUE);
        break;

      default:
        LOCAL_NOT_REACHED("unexpected literal type");
    }

    return builder.literal(val, &pn->pn_pos, dst);
}

bool
ASTSerializer::arrayPattern(JSParseNode *pn, VarDeclKind *pkind, Value *dst)
{
    JS_ASSERT(PN_TYPE(pn) == TOK_RB);

    NodeVector elts(cx);
    if (!elts.reserve(pn->pn_count))
        return false;

    for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
        if (PN_TYPE(next) == TOK_COMMA) {
            elts.infallibleAppend(MagicValue(JS_SERIALIZE_NO_NODE));
        } else {
            Value patt;
            if (!pattern(next, pkind, &patt))
                return false;
            elts.infallibleAppend(patt);
        }
    }

    return builder.arrayPattern(elts, &pn->pn_pos, dst);
}

bool
ASTSerializer::objectPattern(JSParseNode *pn, VarDeclKind *pkind, Value *dst)
{
    JS_ASSERT(PN_TYPE(pn) == TOK_RC);

    NodeVector elts(cx);
    if (!elts.reserve(pn->pn_count))
        return false;

    for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
        LOCAL_ASSERT(PN_OP(next) == JSOP_INITPROP);

        Value key, patt, prop;
        if (!propertyName(next->pn_left, &key) ||
            !pattern(next->pn_right, pkind, &patt) ||
            !builder.propertyPattern(key, patt, &next->pn_pos, &prop)) {
            return false;
        }

        elts.infallibleAppend(prop);
    }

    return builder.objectPattern(elts, &pn->pn_pos, dst);
}

bool
ASTSerializer::pattern(JSParseNode *pn, VarDeclKind *pkind, Value *dst)
{
    JS_CHECK_RECURSION(cx, return false);
    switch (PN_TYPE(pn)) {
      case TOK_RC:
        return objectPattern(pn, pkind, dst);

      case TOK_RB:
        return arrayPattern(pn, pkind, dst);

      case TOK_NAME:
        if (pkind && (pn->pn_dflags & PND_CONST))
            *pkind = VARDECL_CONST;
        /* FALL THROUGH */

      default:
        return expression(pn, dst);
    }
}

bool
ASTSerializer::identifier(JSAtom *atom, TokenPos *pos, Value *dst)
{
    return builder.identifier(atomContents(atom), pos, dst);
}

bool
ASTSerializer::identifier(JSParseNode *pn, Value *dst)
{
    LOCAL_ASSERT(pn->pn_arity == PN_NAME || pn->pn_arity == PN_NULLARY);
    LOCAL_ASSERT(pn->pn_atom);

    return identifier(pn->pn_atom, &pn->pn_pos, dst);
}

bool
ASTSerializer::function(JSParseNode *pn, ASTType type, Value *dst)
{
    JSFunction *func = (JSFunction *)pn->pn_funbox->object;

    bool isGenerator =
#ifdef JS_HAS_GENERATORS
        pn->pn_funbox->tcflags & TCF_FUN_IS_GENERATOR;
#else
        false;
#endif

    bool isExpression =
#ifdef JS_HAS_EXPR_CLOSURES
        func->flags & JSFUN_EXPR_CLOSURE;
#else
        false;
#endif

    Value id;
    if (!optIdentifier(func->atom, NULL, &id))
        return false;

    NodeVector args(cx);

    JSParseNode *argsAndBody = (PN_TYPE(pn->pn_body) == TOK_UPVARS)
                               ? pn->pn_body->pn_tree
                               : pn->pn_body;

    Value body;
    return functionArgsAndBody(argsAndBody, args, &body) &&
           builder.function(type, &pn->pn_pos, id, args, body, isGenerator, isExpression, dst);
}

bool
ASTSerializer::functionArgsAndBody(JSParseNode *pn, NodeVector &args, Value *body)
{
    JSParseNode *pnargs;
    JSParseNode *pnbody;

    /* Extract the args and body separately. */
    if (PN_TYPE(pn) == TOK_ARGSBODY) {
        pnargs = pn;
        pnbody = pn->last();
    } else {
        pnargs = NULL;
        pnbody = pn;
    }

    JSParseNode *pndestruct;

    /* Extract the destructuring assignments. */
    if (pnbody->pn_arity == PN_LIST && (pnbody->pn_xflags & PNX_DESTRUCT)) {
        JSParseNode *head = pnbody->pn_head;
        LOCAL_ASSERT(head && PN_TYPE(head) == TOK_SEMI);

        pndestruct = head->pn_kid;
        LOCAL_ASSERT(pndestruct && PN_TYPE(pndestruct) == TOK_VAR);
    } else {
        pndestruct = NULL;
    }

    /* Serialize the arguments and body. */
    switch (PN_TYPE(pnbody)) {
      case TOK_RETURN: /* expression closure, no destructured args */
        return functionArgs(pn, pnargs, NULL, pnbody, args) &&
               expression(pnbody->pn_kid, body);

      case TOK_SEQ:    /* expression closure with destructured args */
      {
        JSParseNode *pnstart = pnbody->pn_head->pn_next;
        LOCAL_ASSERT(pnstart && PN_TYPE(pnstart) == TOK_RETURN);

        return functionArgs(pn, pnargs, pndestruct, pnbody, args) &&
               expression(pnstart->pn_kid, body);
      }

      case TOK_LC:     /* statement closure */
      {
        JSParseNode *pnstart = (pnbody->pn_xflags & PNX_DESTRUCT)
                               ? pnbody->pn_head->pn_next
                               : pnbody->pn_head;

        return functionArgs(pn, pnargs, pndestruct, pnbody, args) &&
               functionBody(pnstart, &pnbody->pn_pos, body);
      }

      default:
        LOCAL_NOT_REACHED("unexpected function contents");
    }
}

bool
ASTSerializer::functionArgs(JSParseNode *pn, JSParseNode *pnargs, JSParseNode *pndestruct,
                            JSParseNode *pnbody, NodeVector &args)
{
    uint32 i = 0;
    JSParseNode *arg = pnargs ? pnargs->pn_head : NULL;
    JSParseNode *destruct = pndestruct ? pndestruct->pn_head : NULL;
    Value node;

    /*
     * Arguments are found in potentially two different places: 1) the
     * argsbody sequence (which ends with the body node), or 2) a
     * destructuring initialization at the beginning of the body. Loop
     * |arg| through the argsbody and |destruct| through the initial
     * destructuring assignments, stopping only when we've exhausted
     * both.
     */
    while ((arg && arg != pnbody) || destruct) {
        if (destruct && destruct->pn_right->frameSlot() == i) {
            if (!pattern(destruct->pn_left, NULL, &node) || !args.append(node))
                return false;
            destruct = destruct->pn_next;
        } else if (arg && arg != pnbody) {
            /*
             * We don't check that arg->frameSlot() == i since we
             * can't call that method if the arg def has been turned
             * into a use, e.g.:
             *
             *     function(a) { function a() { } }
             *
             * There's no other way to ask a non-destructuring arg its
             * index in the formals list, so we rely on the ability to
             * ask destructuring args their index above.
             */
            if (!identifier(arg, &node) || !args.append(node))
                return false;
            arg = arg->pn_next;
        } else {
            LOCAL_NOT_REACHED("missing function argument");
        }
        ++i;
    }

    return true;
}

bool
ASTSerializer::functionBody(JSParseNode *pn, TokenPos *pos, Value *dst)
{
    NodeVector elts(cx);

    /* We aren't sure how many elements there are up front, so we'll check each append. */
    for (JSParseNode *next = pn; next; next = next->pn_next) {
        Value child;
        if (!sourceElement(next, &child) || !elts.append(child))
            return false;
    }

    return builder.blockStatement(elts, pos, dst);
}

} /* namespace js */

static JSBool
reflect_parse(JSContext *cx, uint32 argc, jsval *vp)
{
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "Reflect.parse", "0", "s");
        return JS_FALSE;
    }

    JSString *src = js_ValueToString(cx, Valueify(JS_ARGV(cx, vp)[0]));
    if (!src)
        return JS_FALSE;

    char *filename = NULL;
    AutoReleaseNullablePtr filenamep(cx, filename);
    uint32 lineno = 1;
    bool loc = true;

    JSObject *builder = NULL;

    Value arg = argc > 1 ? Valueify(JS_ARGV(cx, vp)[1]) : UndefinedValue();

    if (!arg.isNullOrUndefined()) {
        if (!arg.isObject()) {
            js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_UNEXPECTED_TYPE,
                                     JSDVG_SEARCH_STACK, arg, NULL, "not an object", NULL);
            return JS_FALSE;
        }

        JSObject *config = &arg.toObject();

        Value prop;

        /* config.loc */
        if (!GetPropertyDefault(cx, config, ATOM_TO_JSID(cx->runtime->atomState.locAtom),
                                BooleanValue(true), &prop)) {
            return JS_FALSE;
        }

        loc = js_ValueToBoolean(prop);

        if (loc) {
            /* config.source */
            if (!GetPropertyDefault(cx, config, ATOM_TO_JSID(cx->runtime->atomState.sourceAtom),
                                    NullValue(), &prop)) {
                return JS_FALSE;
            }

            if (!prop.isNullOrUndefined()) {
                JSString *str = js_ValueToString(cx, prop);
                if (!str)
                    return JS_FALSE;

                size_t length = str->length();
                const jschar *chars = str->getChars(cx);
                if (!chars)
                    return JS_FALSE;

                filename = DeflateString(cx, chars, length);
                if (!filename)
                    return JS_FALSE;
                filenamep.reset(filename);
            }

            /* config.line */
            if (!GetPropertyDefault(cx, config, ATOM_TO_JSID(cx->runtime->atomState.lineAtom),
                                    Int32Value(1), &prop) ||
                !ValueToECMAUint32(cx, prop, &lineno)) {
                return JS_FALSE;
            }
        }

        /* config.builder */
        if (!GetPropertyDefault(cx, config, ATOM_TO_JSID(cx->runtime->atomState.builderAtom),
                                NullValue(), &prop)) {
            return JS_FALSE;
        }

        if (!prop.isNullOrUndefined()) {
            if (!prop.isObject()) {
                js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_UNEXPECTED_TYPE,
                                         JSDVG_SEARCH_STACK, prop, NULL, "not an object", NULL);
                return JS_FALSE;
            }
            builder = &prop.toObject();
        }
    }

    /* Extract the builder methods first to report errors before parsing. */
    ASTSerializer serialize(cx, loc, filename, lineno);
    if (!serialize.init(builder))
        return JS_FALSE;

    size_t length = src->length();
    const jschar *chars = src->getChars(cx);
    if (!chars)
        return JS_FALSE;

    Parser parser(cx, NULL, NULL, false);

    if (!parser.init(chars, length, filename, lineno, cx->findVersion()))
        return JS_FALSE;

    JSParseNode *pn = parser.parse(NULL);
    if (!pn)
        return JS_FALSE;

    Value val;
    if (!serialize.program(pn, &val)) {
        JS_SET_RVAL(cx, vp, JSVAL_NULL);
        return JS_FALSE;
    }

    JS_SET_RVAL(cx, vp, Jsvalify(val));
    return JS_TRUE;
}

static JSFunctionSpec static_methods[] = {
    JS_FN("parse", reflect_parse, 1, 0),
    JS_FS_END
};


JS_BEGIN_EXTERN_C

JS_PUBLIC_API(JSObject *)
JS_InitReflect(JSContext *cx, JSObject *obj)
{
    JSObject *Reflect = NewNonFunction<WithProto::Class>(cx, &ObjectClass, NULL, obj);
    if (!Reflect || !Reflect->setSingletonType(cx))
        return NULL;

    if (!JS_DefineProperty(cx, obj, "Reflect", OBJECT_TO_JSVAL(Reflect),
                           JS_PropertyStub, JS_StrictPropertyStub, 0)) {
        return NULL;
    }

    if (!JS_DefineFunctions(cx, Reflect, static_methods))
        return NULL;

    return Reflect;
}

JS_END_EXTERN_C
