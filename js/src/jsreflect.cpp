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

using namespace js;

/*
 * These macros cover many of the cases where we really miss
 * exceptions. Because of the do-while trick, these macros
 * must be terminated with a semicolon and are parsed as a
 * single statement, and they avoid subtle bugs where uses of
 * the macros are nested in unbraced if/else-if/else trees.
 */
#define RNULL_UNLESS(e)  do { if (!(e)) return NULL; } while (false)
#define RFALSE_UNLESS(e) do { if (!(e)) return false; } while (false)


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
#define ASTDEF(ast, val, str) str,
#include "jsast.tbl"
#undef ASTDEF
    NULL
};

typedef Vector<JSObject *, 16, ContextAllocPolicy> NodeList;

/*
 * Nullability notation:
 *
 *     N  ::= ? | ! | _
 *     NT ::= N | N* | N+
 *
 * ?:  Nullable pointer or enum that may be -1.
 * !:  Non-nullable pointer or enum that may not be -1.
 * _:  Type that does not include a null case.
 * N*: NodeList with zero or more elements of nullability N.
 * N+: NodeList with one or more elements of nullability N.
 */

typedef struct NodePropertySpec {
    const char *key;
    jsval val;
};

#define PS(key, val) { key, val }
#define PS_END       PS(NULL, JSVAL_VOID)

/*
 * (dst: !, begin: ?, end: ?)
 *
 * modifies result iff begin and end are non-null
 * returns result if modified, null if unmodified
 * infallible
 */
static bool
span(TokenPos *dst, TokenPos *begin, TokenPos *end)
{
    if (!begin || !end)
        return false;

    dst->begin = begin->begin;
    dst->end = end->end;

    return true;
}


/*
 * Bug 569487: generalize builder interface
 *   - use jsval instead of JSObject *
 *   - use JSVAL_HOLE to represent "no value"
 */


/*
 * Builder class that constructs JS ASTNode objects.
 *
 * The constructor is infallible.
 *
 * All methods with pointer or bool return type are fallible.
 *
 * All fallible methods set a pending exception in the associated
 * JSContext before returning.
 */
class NodeBuilder
{
    JSContext    *cx;   /* ! */
    char const   *src;  /* ? */

  public:
    /* (cx: !, src: ?) */
    NodeBuilder(JSContext *c, char const *s)
        : cx(c), src(s) {
    }

  private:
    /* (s: !) */
    /* infallible */
    jsval intern(const char *s) { return STRING_TO_JSVAL(JS_InternString(cx, s)); }

    JSObject *internalError() {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_INTERNAL_ARG);
        return NULL;
    }

    JSObject *makeObject() { return JS_NewObject(cx, NULL, NULL, NULL); }

    /* (props: !) */
    JSObject *makeObject(NodePropertySpec *props);

    /* (childName: !, child: _) */
    JSObject *makeObject(const char *childName, jsval child);

    /* (childName1: !, child1: _, childName2: !, child2: _) */
    JSObject *makeObject(const char *childName1, jsval child1,
                         const char *childName2, jsval child2);


    /* (elts: ?*) */
    JSObject *makeArray(NodeList &elts);

    /* (type: _, pos: ?) */
    JSObject *makeNode(ASTType type, TokenPos *pos);

    /* (type: _, pos: ?, props: !) */
    JSObject *makeNode(ASTType type, TokenPos *pos, NodePropertySpec props[]);


    /* (type: _, pos: ?, childName: !, child: _) */
    JSObject *makeNode(ASTType type, TokenPos *pos, const char *childName, jsval child)
    {
        NodePropertySpec props[] = {
            PS(childName, child),
            PS_END
        };
        return makeNode(type, pos, props);
    }

    /* (type _, pos: ?, childName1: !, child1: _, childName2: !, child2: _) */
    JSObject *makeNode(ASTType type, TokenPos *pos,
                       const char *childName1, jsval child1,
                       const char *childName2, jsval child2) {
        NodePropertySpec props[] = {
            PS(childName1, child1),
            PS(childName2, child2),
            PS_END
        };
        return makeNode(type, pos, props);
    }

    /* (type: _, pos: ?, propName: !, elts: ?*) */
    JSObject *makeListNode(ASTType type, TokenPos *pos, const char *propName, NodeList &elts) {
        JSObject *array = makeArray(elts);
        RNULL_UNLESS(array);
        return makeNode(type, pos, propName, OBJECT_TO_JSVAL(array));
    }

    /* (obj: !, name: !, val: _) */
    bool setObjectProperty(JSObject *obj, const char *name, jsval val) {
        return JS_DefineProperty(cx, obj, name, val, NULL, NULL, JSPROP_ENUMERATE);
    }

    /* (obj: !, props: !) */
    bool setObjectProperties(JSObject *obj, NodePropertySpec *props);

    /* (obj: !, pos: ?) */
    bool setNodeLoc(JSObject *obj, TokenPos *pos);

    /* (dst: !, foundp: !, node: !) */
    bool getNodeLoc(TokenPos *dst, bool *foundp, JSObject *node);

    /* (dst: !, foundp: !, loc: !) */
    bool getTokenPos(TokenPos *dst, bool *foundp, JSObject *loc);

    /* (dst: !, foundp: !, posn: !) */
    bool getTokenPtr(TokenPtr *dst, bool *foundp, JSObject *posn);

    /* (type: _, opType: _, opToken: _, left: !, right: !, pos: ?) */
    JSObject *makeBinaryNode(ASTType type, ASTType opType, jsval opToken,
                             JSObject *left, JSObject *right, TokenPos *pos);

    /* (type: _, opType: _, opToken: _, operands: !+, pos ?) */
    JSObject *leftAssociate(ASTType type, ASTType opType, jsval opToken, NodeList &operands,
                            TokenPos *pos);

  public:
    /*
     * misc nodes
     */

    /* (elts: !*, pos: ?) */
    JSObject *program(NodeList &elts, TokenPos *pos);

    /* (val: _, pos: ?) */
    JSObject *literal(jsval val, TokenPos *pos);

    /* (name: _, pos: ?) */
    JSObject *identifier(jsval name, TokenPos *pos);

    /*
     * (type: _, pos: ?, id: ?, args: !*, body: !, isGenerator: _, isExpression: _)
     */
    JSObject *function(ASTType type, TokenPos *pos,
                       JSObject *id, NodeList &args, JSObject *body,
                       bool isGenerator, bool isExpression);

    /* (id: !, init: ?, pos: ?) */
    JSObject *variableDeclarator(JSObject *id, JSObject *init, TokenPos *pos);

    /* (expr: ?, elts: !*, pos: ?) */
    JSObject *switchCase(JSObject *expr, NodeList &elts, TokenPos *pos);

    /* (var: !, guard: ?, body: !, pos: ?) */
    JSObject *catchClause(JSObject *var, JSObject *guard, JSObject *body, TokenPos *pos);

    /* (key: !, val: !, kind: !, pos: ?) */
    JSObject *propertyInitializer(JSObject *key, JSObject *val, PropKind kind, TokenPos *pos);


    /*
     * statements
     */

    /* (elts: !*, pos: ?) */
    JSObject *blockStatement(NodeList &elts, TokenPos *pos);

    /* (expr: !, pos: ?) */
    JSObject *expressionStatement(JSObject *expr, TokenPos *pos);

    /* (pos: ?) */
    JSObject *emptyStatement(TokenPos *pos);

    /* (test: !, cons: !, alt: ?, pos: ?) */
    JSObject *ifStatement(JSObject *test, JSObject *cons, JSObject *alt, TokenPos *pos);

    /* (label: ?, pos: ?) */
    JSObject *breakStatement(JSObject *label, TokenPos *pos);

    /* (label: ?, pos: ?) */
    JSObject *continueStatement(JSObject *label, TokenPos *pos);

    /* (label: !, pos: ?) */
    JSObject *labeledStatement(JSObject *label, JSObject *stmt, TokenPos *pos);

    /* (arg: !, pos: ?) */
    JSObject *throwStatement(JSObject *arg, TokenPos *pos);

    /* (arg: ?, pos: ?) */
    JSObject *returnStatement(JSObject *arg, TokenPos *pos);

    /* (init: ?, test: ?, update: ?, stmt: !, pos: ?) */
    JSObject *forStatement(JSObject *init, JSObject *test, JSObject *update,
                           JSObject *stmt, TokenPos *pos);

    /* (var: !, expr: !, stmt: !, isForEach: _, pos: ?) */
    JSObject *forInStatement(JSObject *var, JSObject *expr, JSObject *stmt,
                             bool isForEach, TokenPos *pos);

    /* (expr: !, stmt: !, pos: ?) */
    JSObject *withStatement(JSObject *expr, JSObject *stmt, TokenPos *pos);

    /* (test: !, stmt: !, pos: ?) */
    JSObject *whileStatement(JSObject *test, JSObject *stmt, TokenPos *pos);

    /* (stmt: !, test: !, pos: ?) */
    JSObject *doWhileStatement(JSObject *stmt, JSObject *test, TokenPos *pos);

    /* (disc: !, elts: !*, lexical: _, pos: ?) */
    JSObject *switchStatement(JSObject *disc, NodeList &elts, bool lexical, TokenPos *pos);

    /* (body: !, catches: !*, finally: ?, pos: ?) */
    JSObject *tryStatement(JSObject *body, NodeList &catches, JSObject *finally, TokenPos *pos);

    /* (pos: ?) */
    JSObject *debuggerStatement(TokenPos *pos);

    /*
     * expressions
     */

    /* (op: !, operands: !+, pos: ?) */
    JSObject *binaryExpression(BinaryOperator op, NodeList &operands, TokenPos *pos);

    /* (op: !, expr: !, pos: ?) */
    JSObject *unaryExpression(UnaryOperator op, JSObject *expr, TokenPos *pos);

    /* (op: !, lhs: !, rhs: !, pos: ?) */
    JSObject *assignmentExpression(AssignmentOperator op, JSObject *lhs, JSObject *rhs,
                                   TokenPos *pos);

    /* (expr: ?, incr: _, prefix: _, pos: ?) */
    JSObject *updateExpression(JSObject *expr, bool incr, bool prefix, TokenPos *pos);

    /* (lor: _, operands: !+, pos: ?) */
    JSObject *logicalExpression(bool lor, NodeList &operands, TokenPos *pos);

    /* (test: !, cons: !, alt: !, pos: ?) */
    JSObject *conditionalExpression(JSObject *test, JSObject *cons, JSObject *alt, TokenPos *pos);

    /* (elts: !+, pos: ?) */
    JSObject *sequenceExpression(NodeList &elts, TokenPos *pos);

    /* (callee: !, args: !*, pos: ?) */
    JSObject *newExpression(JSObject *callee, NodeList &args, TokenPos *pos);

    /* (callee: !, args: !*, pos: ?) */
    JSObject *callExpression(JSObject *callee, NodeList &args, TokenPos *pos);

    /* (computed: _, expr: !, member: !, pos: ?) */
    JSObject *memberExpression(bool computed, JSObject *expr, JSObject *member, TokenPos *pos);

    /* (elts: ?*, pos: ?) */
    JSObject *arrayExpression(NodeList &elts, TokenPos *pos);

    /* (elts: !, pos: ?) */
    JSObject *objectExpression(NodeList &elts, TokenPos *pos);

    /* (arg: ?, pos: ?) */
    JSObject *yieldExpression(JSObject *arg, TokenPos *pos);

    /* (patt: !, src: !, isForEach: _, pos: ?) */
    JSObject *comprehensionBlock(JSObject *patt, JSObject *src, bool isForEach, TokenPos *pos);

    /* (body: !, blocks: !+, filter: ?, pos: ?) */
    JSObject *comprehensionExpression(JSObject *body, NodeList &blocks, JSObject *filter,
                                      TokenPos *pos);

    /* (body: !, blocks: !+, filter: ?, pos: ?) */
    JSObject *generatorExpression(JSObject *body, NodeList &blocks, JSObject *filter,
                                  TokenPos *pos);

    /* (idx: _, expr: !, pos: ?) */
    JSObject *graphExpression(jsint idx, JSObject *expr, TokenPos *pos);

    /* (idx: _, pos: ?) */
    JSObject *graphIndexExpression(jsint idx, TokenPos *pos);

    /*
     * declarations
     */

    /* (elts: !+, kind: !, pos: ?) */
    JSObject *variableDeclaration(NodeList &elts, VarDeclKind kind, TokenPos *pos);


    /*
     * patterns
     */

    /* (elts: ?*, pos: ?) */
    JSObject *arrayPattern(NodeList &elts, TokenPos *pos);

    /* (elts: !*, pos: ?) */
    JSObject *objectPattern(NodeList &elts, TokenPos *pos);

    /* (key: !, patt: !, pos: ?) */
    JSObject *propertyPattern(JSObject *key, JSObject *patt, TokenPos *pos);


    /*
     * xml
     */

    /* (pos: ?) */
    JSObject *xmlAnyName(TokenPos *pos);

    /* (expr: !, pos: ?) */
    JSObject *xmlEscapeExpression(JSObject *expr, TokenPos *pos);

    /* (ns: !, pos: ?) */
    JSObject *xmlDefaultNamespace(JSObject *ns, TokenPos *pos);

    /* (left: !, right: !, pos: ?) */
    JSObject *xmlFilterExpression(JSObject *left, JSObject *right, TokenPos *pos);

    /* (expr: !, pos: ?) */
    JSObject *xmlAttributeSelector(JSObject *expr, TokenPos *pos);

    /* (left: !, right: !, computed: _, pos: ?) */
    JSObject *xmlQualifiedIdentifier(JSObject *left, JSObject *right, bool computed,
                                     TokenPos *pos);

    /* (elts: !+, pos: ?) */
    JSObject *xmlElement(NodeList &elts, TokenPos *pos);

    /* (text: _, pos: ?) */
    JSObject *xmlText(jsval text, TokenPos *pos);

    /* (elts: !+, pos: ?) */
    JSObject *xmlList(NodeList &elts, TokenPos *pos);

    /* (elts: !+, pos: ?) */
    JSObject *xmlStartTag(NodeList &elts, TokenPos *pos);

    /* (elts: !+, pos: ?) */
    JSObject *xmlEndTag(NodeList &elts, TokenPos *pos);

    /* (elts: !+, pos: ?) */
    JSObject *xmlPointTag(NodeList &elts, TokenPos *pos);

    /* (text: _, pos: ?) */
    JSObject *xmlName(jsval text, TokenPos *pos);

    /* (elts: !+, pos: ?) */
    JSObject *xmlName(NodeList &elts, TokenPos *pos);

    /* (text: _, pos: ?) */
    JSObject *xmlAttribute(jsval text, TokenPos *pos);

    /* (text: _, pos: ?) */
    JSObject *xmlCdata(jsval text, TokenPos *pos);

    /* (text: _, pos: ?) */
    JSObject *xmlComment(jsval text, TokenPos *pos);

    /* (target: _, pos: ?) */
    JSObject *xmlPI(jsval target, TokenPos *pos);

    /* (target: _, content: _, pos: ?) */
    JSObject *xmlPI(jsval target, jsval content, TokenPos *pos);

};

JSObject *
NodeBuilder::makeObject(NodePropertySpec *props)
{
    JSObject *obj = makeObject();
    RNULL_UNLESS(obj);
    RNULL_UNLESS(setObjectProperties(obj, props));
    return obj;
}

JSObject *
NodeBuilder::makeObject(const char *childName, jsval child)
{
    NodePropertySpec props[] = {
        PS(childName, child),
        PS_END
    };
    return makeObject(props);
}

JSObject *
NodeBuilder::makeObject(const char *childName1, jsval child1,
                        const char *childName2, jsval child2)
{
    NodePropertySpec props[] = {
        PS(childName1, child1),
        PS(childName2, child2),
        PS_END
    };
    return makeObject(props);
}

bool
NodeBuilder::setObjectProperties(JSObject *obj, NodePropertySpec *props)
{
    for (size_t i = 0; props[i].key; i++)
        RFALSE_UNLESS(setObjectProperty(obj, props[i].key, props[i].val));

    return true;
}

JSObject *
NodeBuilder::makeNode(ASTType type, TokenPos *pos)
{
    if (type <= AST_ERROR || type >= AST_LIMIT)
        return internalError();

    JSObject *node = JS_NewObject(cx, &js_ASTNodeClass, NULL, NULL);
    RNULL_UNLESS(node);
    RNULL_UNLESS(setNodeLoc(node, pos));
    RNULL_UNLESS(setObjectProperty(node, "type", intern(nodeTypeNames[type])));
    return node;
}

JSObject *
NodeBuilder::makeNode(ASTType type, TokenPos *pos, NodePropertySpec props[])
{
    JSObject *node = makeNode(type, pos);
    RNULL_UNLESS(node);
    for (size_t i = 0; props[i].key; i++)
        RNULL_UNLESS(setObjectProperty(node, props[i].key, props[i].val));
    return node;
}

JSObject *
NodeBuilder::makeArray(NodeList &elts)
{
    JSObject *array = JS_NewArrayObject(cx, 0, NULL);
    RNULL_UNLESS(array);

    size_t len = elts.length();
    for (size_t i = 0; i < len; i++) {
        jsval val = OBJECT_TO_JSVAL(elts[i]);
        RNULL_UNLESS(JS_SetElement(cx, array, i, &val));
    }

    return array;
}

bool
NodeBuilder::setNodeLoc(JSObject *node, TokenPos *pos)
{
    if (!pos)
        return setObjectProperty(node, "loc", JSVAL_NULL);

    jsval line, column;

    RFALSE_UNLESS(JS_NewNumberValue(cx, (jsdouble)pos->begin.lineno, &line));
    RFALSE_UNLESS(JS_NewNumberValue(cx, (jsdouble)pos->begin.index, &column));
    JSObject *start = makeObject("line", line, "column", column);
    RFALSE_UNLESS(start);

    RFALSE_UNLESS(JS_NewNumberValue(cx, (jsdouble)pos->end.lineno, &line));
    RFALSE_UNLESS(JS_NewNumberValue(cx, (jsdouble)pos->end.index, &column));
    JSObject *end = makeObject("line", line, "column", column);
    RFALSE_UNLESS(end);

    JSObject *loc = makeObject("start", OBJECT_TO_JSVAL(start),
                               "end", OBJECT_TO_JSVAL(end));
    RFALSE_UNLESS(loc);

    return setObjectProperty(node, "loc", OBJECT_TO_JSVAL(loc));
}

JSObject *
NodeBuilder::program(NodeList &elts, TokenPos *pos)
{
    JSObject *array = makeArray(elts);
    RNULL_UNLESS(array);
    return makeNode(AST_PROGRAM, pos, "body", OBJECT_TO_JSVAL(array));
}

JSObject *
NodeBuilder::blockStatement(NodeList &elts, TokenPos *pos)
{
    JSObject *array = makeArray(elts);
    RNULL_UNLESS(array);

    return makeNode(AST_BLOCK_STMT, pos, "body", OBJECT_TO_JSVAL(array));
}

JSObject *
NodeBuilder::expressionStatement(JSObject *expr, TokenPos *pos)
{
    return makeNode(AST_EXPR_STMT, pos, "expression", OBJECT_TO_JSVAL(expr));
}

JSObject *
NodeBuilder::emptyStatement(TokenPos *pos)
{
    return makeNode(AST_EMPTY_STMT, pos);
}

JSObject *
NodeBuilder::ifStatement(JSObject *test, JSObject *cons, JSObject *alt, TokenPos *pos)
{
    NodePropertySpec props[] = {
        PS("test", OBJECT_TO_JSVAL(test)),
        PS("consequent", OBJECT_TO_JSVAL(cons)),
        PS("alternate", alt ? OBJECT_TO_JSVAL(alt) : JSVAL_NULL),
        PS_END
    };
    return makeNode(AST_IF_STMT, pos, props);
}

JSObject *
NodeBuilder::breakStatement(JSObject *label, TokenPos *pos)
{
    return makeNode(AST_BREAK_STMT, pos, "label", label ? OBJECT_TO_JSVAL(label) : JSVAL_NULL);
}

JSObject *
NodeBuilder::continueStatement(JSObject *label, TokenPos *pos)
{
    return makeNode(AST_CONTINUE_STMT, pos, "label", label ? OBJECT_TO_JSVAL(label) : JSVAL_NULL);
}

JSObject *
NodeBuilder::labeledStatement(JSObject *label, JSObject *stmt, TokenPos *pos)
{
    return makeNode(AST_LAB_STMT, pos,
                    "label", OBJECT_TO_JSVAL(label),
                    "body", OBJECT_TO_JSVAL(stmt));
}

JSObject *
NodeBuilder::throwStatement(JSObject *arg, TokenPos *pos)
{
    return makeNode(AST_THROW_STMT, pos, "argument", OBJECT_TO_JSVAL(arg));
}

JSObject *
NodeBuilder::returnStatement(JSObject *arg, TokenPos *pos)
{
    return makeNode(AST_RETURN_STMT, pos, "argument", arg ? OBJECT_TO_JSVAL(arg) : JSVAL_NULL);
}

JSObject *
NodeBuilder::forStatement(JSObject *init, JSObject *test, JSObject *update,
                          JSObject *stmt, TokenPos *pos)
{
    NodePropertySpec props[] = {
        PS("init", init ? OBJECT_TO_JSVAL(init) : JSVAL_NULL),
        PS("test", test ? OBJECT_TO_JSVAL(test) : JSVAL_NULL),
        PS("update", update ? OBJECT_TO_JSVAL(update) : JSVAL_NULL),
        PS("body", OBJECT_TO_JSVAL(stmt)),
        PS_END
    };
    return makeNode(AST_FOR_STMT, pos, props);
}

JSObject *
NodeBuilder::forInStatement(JSObject *var, JSObject *expr, JSObject *stmt,
                            bool isForEach, TokenPos *pos)
{
    NodePropertySpec props[] = {
        PS("left", OBJECT_TO_JSVAL(var)),
        PS("right", OBJECT_TO_JSVAL(expr)),
        PS("body", OBJECT_TO_JSVAL(stmt)),
        PS("each", BOOLEAN_TO_JSVAL(isForEach)),
        PS_END
    };
    return makeNode(AST_FOR_IN_STMT, pos, props);
}

JSObject *
NodeBuilder::withStatement(JSObject *expr, JSObject *stmt, TokenPos *pos)
{
    return makeNode(AST_WITH_STMT, pos,
                    "object", OBJECT_TO_JSVAL(expr),
                    "body", OBJECT_TO_JSVAL(stmt));
}

JSObject *
NodeBuilder::whileStatement(JSObject *test, JSObject *stmt, TokenPos *pos)
{
    return makeNode(AST_WHILE_STMT, pos,
                    "test", OBJECT_TO_JSVAL(test),
                    "body", OBJECT_TO_JSVAL(stmt));
}

JSObject *
NodeBuilder::doWhileStatement(JSObject *stmt, JSObject *test, TokenPos *pos)
{
    return makeNode(AST_DO_STMT, pos,
                    "body", OBJECT_TO_JSVAL(stmt),
                    "test", OBJECT_TO_JSVAL(test));
}

JSObject *
NodeBuilder::switchStatement(JSObject *disc, NodeList &elts, bool lexical, TokenPos *pos)
{
    JSObject *array = makeArray(elts);
    RNULL_UNLESS(array);

    JSObject *meta = makeObject("lexical", BOOLEAN_TO_JSVAL(lexical));
    RNULL_UNLESS(meta);

    NodePropertySpec props[] = {
        PS("discriminant", OBJECT_TO_JSVAL(disc)),
        PS("cases", OBJECT_TO_JSVAL(array)),
        PS("meta", OBJECT_TO_JSVAL(meta)),
        PS_END
    };
    return makeNode(AST_SWITCH_STMT, pos, props);
}

JSObject *
NodeBuilder::tryStatement(JSObject *body, NodeList &catches, JSObject *finally, TokenPos *pos)
{
    jsval handler;
    if (catches.empty()) {
        handler = JSVAL_NULL;
    } else if (catches.length() == 1) {
        handler = OBJECT_TO_JSVAL(catches[0]);
    } else {
        JSObject *array = makeArray(catches);
        RNULL_UNLESS(array);
        handler = OBJECT_TO_JSVAL(array);
    }

    NodePropertySpec props[] = {
        PS("block", OBJECT_TO_JSVAL(body)),
        PS("handler", handler),
        PS("finalizer", finally ? OBJECT_TO_JSVAL(finally) : JSVAL_NULL),
        PS_END
    };
    return makeNode(AST_TRY_STMT, pos, props);
}

JSObject *
NodeBuilder::debuggerStatement(TokenPos *pos)
{
    return makeNode(AST_DEBUGGER_STMT, pos);
}

JSObject *
NodeBuilder::binaryExpression(BinaryOperator op, NodeList &operands, TokenPos *pos)
{
    if (op <= BINOP_ERR || op >= BINOP_LIMIT)
        return internalError();

    return leftAssociate(AST_BINARY_EXPR, AST_BINOP, intern(binopNames[op]), operands, pos);
}

JSObject *
NodeBuilder::unaryExpression(UnaryOperator op, JSObject *expr, TokenPos *pos)
{
    if (op <= UNOP_ERR || op >= UNOP_LIMIT)
        return internalError();

    JSObject *opNode = makeNode(AST_UNOP, NULL, "token", intern(unopNames[op]));
    RNULL_UNLESS(opNode);

    NodePropertySpec props[] = {
        PS("operator", OBJECT_TO_JSVAL(opNode)),
        PS("argument", OBJECT_TO_JSVAL(expr)),
        PS("prefix", JSVAL_TRUE),
        PS_END
    };
    return makeNode(AST_UNARY_EXPR, pos, props);
}

JSObject *
NodeBuilder::assignmentExpression(AssignmentOperator op, JSObject *lhs, JSObject *rhs,
                                  TokenPos *pos)
{
    if (op <= AOP_ERR || op >= AOP_LIMIT)
        return internalError();

    JSObject *opNode = makeNode(AST_AOP, NULL, "token", intern(aopNames[op]));
    RNULL_UNLESS(opNode);

    NodePropertySpec props[] = {
        PS("operator", OBJECT_TO_JSVAL(opNode)),
        PS("left", OBJECT_TO_JSVAL(lhs)),
        PS("right", OBJECT_TO_JSVAL(rhs)),
        PS_END
    };
    return makeNode(AST_ASSIGN_EXPR, pos, props);
}

JSObject *
NodeBuilder::updateExpression(JSObject *expr, bool incr, bool prefix, TokenPos *pos)
{
    JSObject *opNode = makeNode(AST_UPDOP, NULL, "token", intern(incr ? "++" : "--"));
    RNULL_UNLESS(opNode);

    NodePropertySpec props[] = {
        PS("operator", OBJECT_TO_JSVAL(opNode)),
        PS("argument", OBJECT_TO_JSVAL(expr)),
        PS("prefix", BOOLEAN_TO_JSVAL(prefix)),
        PS_END
    };
    return makeNode(AST_UPDATE_EXPR, pos, props);
}

bool
NodeBuilder::getTokenPtr(TokenPtr *dst, bool *foundp, JSObject *posn)
{
    JSBool has;

    RFALSE_UNLESS(JS_HasProperty(cx, posn, "line", &has));
    if (!has) {
        *foundp = false;
        return true;
    }

    RFALSE_UNLESS(JS_HasProperty(cx, posn, "column", &has));
    if (!has) {
        *foundp = false;
        return true;
    }

    jsval linev, columnv;
    RFALSE_UNLESS(JS_GetProperty(cx, posn, "line", &linev));
    RFALSE_UNLESS(JS_GetProperty(cx, posn, "column", &columnv));

    if (!JSVAL_IS_NUMBER(linev) || !JSVAL_IS_NUMBER(columnv)) {
        *foundp = false;
        return true;
    }

    uint32 line, column;
    RFALSE_UNLESS(JS_ValueToECMAUint32(cx, linev, &line));
    RFALSE_UNLESS(JS_ValueToECMAUint32(cx, columnv, &column));

    dst->lineno = line;
    dst->index = column;

    *foundp = true;
    return true;
}

bool
NodeBuilder::getTokenPos(TokenPos *dst, bool *foundp, JSObject *loc)
{
    JSBool has;

    RFALSE_UNLESS(JS_HasProperty(cx, loc, "start", &has));
    if (!has) {
        *foundp = false;
        return true;
    }

    RFALSE_UNLESS(JS_HasProperty(cx, loc, "end", &has));
    if (!has) {
        *foundp = false;
        return true;
    }

    jsval startv, endv;
    RFALSE_UNLESS(JS_GetProperty(cx, loc, "start", &startv));
    RFALSE_UNLESS(JS_GetProperty(cx, loc, "end", &endv));

    if (!JSVAL_IS_OBJECT(startv) || !JSVAL_IS_OBJECT(endv)) {
        *foundp = false;
        return true;
    }

    TokenPtr begin, end;
    RFALSE_UNLESS(getTokenPtr(&begin, foundp, JSVAL_TO_OBJECT(startv)));
    if (!(*foundp))
        return true;

    RFALSE_UNLESS(getTokenPtr(&end, foundp, JSVAL_TO_OBJECT(endv)));
    if (!(*foundp))
        return true;

    dst->begin = begin;
    dst->end = end;

    *foundp = true;
    return true;
}

bool
NodeBuilder::getNodeLoc(TokenPos *dst, bool *foundp, JSObject *node)
{
    JSBool has;
    RFALSE_UNLESS(JS_HasProperty(cx, node, "loc", &has));
    if (!has) {
        *foundp = false;
        return true;
    }

    jsval loc;
    RFALSE_UNLESS(JS_GetProperty(cx, node, "loc", &loc));

    if (!JSVAL_IS_OBJECT(loc)) {
        *foundp = false;
        return true;
    }

    return getTokenPos(dst, foundp, JSVAL_TO_OBJECT(loc));
}

JSObject *
NodeBuilder::makeBinaryNode(ASTType type, ASTType opType, jsval opToken,
                            JSObject *left, JSObject *right, TokenPos *pos)
{
    JSObject *opNode = makeNode(opType, NULL, "token", opToken);
    RNULL_UNLESS(opNode);

    NodePropertySpec props[] = {
        PS("operator", OBJECT_TO_JSVAL(opNode)),
        PS("left", OBJECT_TO_JSVAL(left)),
        PS("right", OBJECT_TO_JSVAL(right)),
        PS_END
    };

    TokenPos leftpos, subpos;
    bool found;

    RNULL_UNLESS(getNodeLoc(&leftpos, &found, left));
    if (found)
        found = span(&subpos, &leftpos, pos);

    return makeNode(type, found ? &subpos : pos, props);
}

JSObject *
NodeBuilder::leftAssociate(ASTType type, ASTType opType, jsval opToken, NodeList &operands,
                           TokenPos *pos)
{
    size_t len = operands.length();

    JS_ASSERT(len >= 1);
    if (len == 1)
        return operands[0];

    JS_ASSERT(len >= 2);

    JSObject *result = operands[len - 1];
    size_t i = len - 2;

    do {
        result = makeBinaryNode(type, opType, opToken, operands[i], result, pos);
        RNULL_UNLESS(result);
    } while (i-- != 0);

    return result;
}

JSObject *
NodeBuilder::logicalExpression(bool lor, NodeList &operands, TokenPos *pos)
{
    return leftAssociate(AST_LOGICAL_EXPR, AST_LOGOP, intern(lor ? "||" : "&&"), operands, pos);
}

JSObject *
NodeBuilder::conditionalExpression(JSObject *test, JSObject *cons, JSObject *alt, TokenPos *pos)
{
    NodePropertySpec props[] = {
        PS("test", OBJECT_TO_JSVAL(test)),
        PS("consequent", OBJECT_TO_JSVAL(cons)),
        PS("alternate", OBJECT_TO_JSVAL(alt)),
        PS_END
    };
    return makeNode(AST_COND_EXPR, pos, props);
}

JSObject *
NodeBuilder::sequenceExpression(NodeList &elts, TokenPos *pos)
{
    JSObject *array = makeArray(elts);
    RNULL_UNLESS(array);

    return makeNode(AST_LIST_EXPR, pos, "expressions", OBJECT_TO_JSVAL(array));
}

JSObject *
NodeBuilder::callExpression(JSObject *callee, NodeList &args, TokenPos *pos)
{
    JSObject *array = makeArray(args);
    RNULL_UNLESS(array);

    return makeNode(AST_CALL_EXPR, pos,
                    "callee", OBJECT_TO_JSVAL(callee),
                    "arguments", OBJECT_TO_JSVAL(array));
}

JSObject *
NodeBuilder::newExpression(JSObject *callee, NodeList &args, TokenPos *pos)
{
    JSObject *array = makeArray(args);
    RNULL_UNLESS(array);

    return makeNode(AST_NEW_EXPR, pos,
                    "callee", OBJECT_TO_JSVAL(callee),
                    "arguments", OBJECT_TO_JSVAL(array));
}

JSObject *
NodeBuilder::memberExpression(bool computed, JSObject *expr, JSObject *member, TokenPos *pos)
{
    NodePropertySpec props[] = {
        PS("object", OBJECT_TO_JSVAL(expr)),
        PS("property", OBJECT_TO_JSVAL(member)),
        PS("computed", BOOLEAN_TO_JSVAL(computed)),
        PS_END
    };
    return makeNode(AST_MEMBER_EXPR, pos, props);
}

JSObject *
NodeBuilder::arrayExpression(NodeList &elts, TokenPos *pos)
{
    JSObject *array = makeArray(elts);
    RNULL_UNLESS(array);

    return makeNode(AST_ARRAY_EXPR, pos, "elements", OBJECT_TO_JSVAL(array));
}

JSObject *
NodeBuilder::propertyPattern(JSObject *key, JSObject *patt, TokenPos *pos)
{
    return makeObject("key", OBJECT_TO_JSVAL(key),
                      "value", OBJECT_TO_JSVAL(patt));
}

JSObject *
NodeBuilder::propertyInitializer(JSObject *key, JSObject *val, PropKind kind, TokenPos *pos)
{
    NodePropertySpec props[] = {
        PS("key", OBJECT_TO_JSVAL(key)),
        PS("value", OBJECT_TO_JSVAL(val)),
        PS("kind", intern(kind == PROP_INIT
                          ? "init"
                          : kind == PROP_GETTER
                          ? "get"
                          : "set")),
        PS_END
    };
    return makeObject(props);
}

JSObject *
NodeBuilder::objectExpression(NodeList &elts, TokenPos *pos)
{
    JSObject *array = makeArray(elts);
    RNULL_UNLESS(array);

    return makeNode(AST_OBJECT_EXPR, pos, "properties", OBJECT_TO_JSVAL(array));
}

JSObject *
NodeBuilder::yieldExpression(JSObject *arg, TokenPos *pos)
{
    return makeNode(AST_YIELD_EXPR, pos, "argument", arg ? OBJECT_TO_JSVAL(arg) : JSVAL_NULL);
}

JSObject *
NodeBuilder::comprehensionBlock(JSObject *patt, JSObject *src, bool isForEach, TokenPos *pos)
{
    NodePropertySpec props[] = {
        PS("left", OBJECT_TO_JSVAL(patt)),
        PS("right", OBJECT_TO_JSVAL(src)),
        PS("each", BOOLEAN_TO_JSVAL(isForEach)),
        PS_END
    };
    return makeNode(AST_COMP_BLOCK, pos, props);
}

JSObject *
NodeBuilder::comprehensionExpression(JSObject *body, NodeList &blocks, JSObject *filter,
                                     TokenPos *pos)
{
    JSObject *array = makeArray(blocks);
    RNULL_UNLESS(array);

    NodePropertySpec props[] = {
        PS("body", OBJECT_TO_JSVAL(body)),
        PS("blocks", OBJECT_TO_JSVAL(array)),
        PS("filter", filter ? OBJECT_TO_JSVAL(filter) : JSVAL_NULL),
        PS_END
    };
    return makeNode(AST_COMP_EXPR, pos, props);
}

JSObject *
NodeBuilder::generatorExpression(JSObject *body, NodeList &blocks, JSObject *filter, TokenPos *pos)
{
    JSObject *array = makeArray(blocks);
    RNULL_UNLESS(array);

    NodePropertySpec props[] = {
        PS("body", OBJECT_TO_JSVAL(body)),
        PS("blocks", OBJECT_TO_JSVAL(array)),
        PS("filter", filter ? OBJECT_TO_JSVAL(filter) : JSVAL_NULL),
        PS_END
    };
    return makeNode(AST_GENERATOR_EXPR, pos, props);
}

JSObject *
NodeBuilder::graphExpression(jsint idx, JSObject *expr, TokenPos *pos)
{
    jsval idxv;

    RNULL_UNLESS(JS_NewNumberValue(cx, (jsdouble)idx, &idxv));

    return makeNode(AST_GRAPH_EXPR, pos,
                    "index", idxv,
                    "expression", OBJECT_TO_JSVAL(expr));
}

JSObject *
NodeBuilder::graphIndexExpression(jsint idx, TokenPos *pos)
{
    jsval idxv;

    RNULL_UNLESS(JS_NewNumberValue(cx, (jsdouble)idx, &idxv));

    return makeNode(AST_GRAPH_IDX_EXPR, pos, "index", idxv);
}

JSObject *
NodeBuilder::variableDeclaration(NodeList &elts, VarDeclKind kind, TokenPos *pos)
{
    if (kind <= VARDECL_ERR || kind >= VARDECL_LIMIT)
        return internalError();

    JSObject *array = makeArray(elts);
    RNULL_UNLESS(array);

    return makeNode(AST_VAR_DECL, pos,
                    "declarations", OBJECT_TO_JSVAL(array),
                    "kind", kind == VARDECL_CONST
                            ? intern("const")
                            : kind == VARDECL_LET
                            ? intern("let")
                            : intern("var"));
}

JSObject *
NodeBuilder::variableDeclarator(JSObject *id, JSObject *init, TokenPos *pos)
{
    return makeObject("id", OBJECT_TO_JSVAL(id),
                      "init", init ? OBJECT_TO_JSVAL(init) : JSVAL_NULL);
}

JSObject *
NodeBuilder::switchCase(JSObject *expr, NodeList &elts, TokenPos *pos)
{
    JSObject *array = makeArray(elts);
    RNULL_UNLESS(array);

    return makeNode(AST_CASE, pos,
                    "test", expr ? OBJECT_TO_JSVAL(expr) : JSVAL_NULL,
                    "consequent", OBJECT_TO_JSVAL(array));
}

JSObject *
NodeBuilder::catchClause(JSObject *var, JSObject *guard, JSObject *body, TokenPos *pos)
{
    NodePropertySpec props[] = {
        PS("param", OBJECT_TO_JSVAL(var)),
        PS("guard", guard ? OBJECT_TO_JSVAL(guard) : JSVAL_NULL),
        PS("body", OBJECT_TO_JSVAL(body)),
        PS_END
    };
    return makeNode(AST_CATCH, pos, props);
}

JSObject *
NodeBuilder::literal(jsval val, TokenPos *pos)
{
    return makeNode(AST_LITERAL, pos, "value", val);
}

JSObject *
NodeBuilder::identifier(jsval name, TokenPos *pos)
{
    return makeNode(AST_IDENTIFIER, pos, "name", name);
}

JSObject *
NodeBuilder::objectPattern(NodeList &elts, TokenPos *pos)
{
    JSObject *array = makeArray(elts);
    RNULL_UNLESS(array);
    return makeNode(AST_OBJECT_PATT, pos, "properties", OBJECT_TO_JSVAL(array));
}

JSObject *
NodeBuilder::arrayPattern(NodeList &elts, TokenPos *pos)
{
    JSObject *array = makeArray(elts);
    RNULL_UNLESS(array);
    return makeNode(AST_ARRAY_PATT, pos, "elements", OBJECT_TO_JSVAL(array));
}

JSObject *
NodeBuilder::function(ASTType type, TokenPos *pos,
                      JSObject *id, NodeList &args, JSObject *body,
                      bool isGenerator, bool isExpression)
{
    JSObject *argsArray = makeArray(args);
    RNULL_UNLESS(argsArray);

    NodePropertySpec props[] = {
        PS("id", id ? OBJECT_TO_JSVAL(id) : JSVAL_NULL),
        PS("params", OBJECT_TO_JSVAL(argsArray)),
        PS("body", OBJECT_TO_JSVAL(body)),
        PS("generator", BOOLEAN_TO_JSVAL(isGenerator)),
        PS("expression", BOOLEAN_TO_JSVAL(isExpression)),
        PS_END
    };
    return makeNode(type, pos, props);
}

JSObject *
NodeBuilder::xmlAnyName(TokenPos *pos)
{
    return makeNode(AST_XMLANYNAME, pos);
}

JSObject *
NodeBuilder::xmlEscapeExpression(JSObject *expr, TokenPos *pos)
{
    return makeNode(AST_XMLESCAPE, pos, "expression", OBJECT_TO_JSVAL(expr));
}

JSObject *
NodeBuilder::xmlFilterExpression(JSObject *left, JSObject *right, TokenPos *pos)
{
    return makeNode(AST_XMLFILTER, pos,
                    "left", OBJECT_TO_JSVAL(left),
                    "right", OBJECT_TO_JSVAL(right));
}

JSObject *
NodeBuilder::xmlDefaultNamespace(JSObject *ns, TokenPos *pos)
{
    return makeNode(AST_XMLDEFAULT, pos, "namespace", OBJECT_TO_JSVAL(ns));
}

JSObject *
NodeBuilder::xmlAttributeSelector(JSObject *expr, TokenPos *pos)
{
    return makeNode(AST_XMLATTR_SEL, pos, "attribute", OBJECT_TO_JSVAL(expr));
}

JSObject *
NodeBuilder::xmlQualifiedIdentifier(JSObject *left, JSObject *right, bool computed, TokenPos *pos)
{
    NodePropertySpec props[] = {
        PS("left", OBJECT_TO_JSVAL(left)),
        PS("right", OBJECT_TO_JSVAL(right)),
        PS("computed", BOOLEAN_TO_JSVAL(computed)),
        PS_END
    };
    return makeNode(AST_XMLQUAL, pos, props);
}

JSObject *
NodeBuilder::xmlElement(NodeList &elts, TokenPos *pos)
{
    return makeListNode(AST_XMLELEM, pos, "contents", elts);
}

JSObject *
NodeBuilder::xmlText(jsval text, TokenPos *pos)
{
    return makeNode(AST_XMLTEXT, pos, "text", text);
}

JSObject *
NodeBuilder::xmlList(NodeList &elts, TokenPos *pos)
{
    return makeListNode(AST_XMLLIST, pos, "contents", elts);
}

JSObject *
NodeBuilder::xmlStartTag(NodeList &elts, TokenPos *pos)
{
    return makeListNode(AST_XMLSTART, pos, "contents", elts);
}

JSObject *
NodeBuilder::xmlEndTag(NodeList &elts, TokenPos *pos)
{
    return makeListNode(AST_XMLEND, pos, "contents", elts);
}

JSObject *
NodeBuilder::xmlPointTag(NodeList &elts, TokenPos *pos)
{
    return makeListNode(AST_XMLPOINT, pos, "contents", elts);
}

JSObject *
NodeBuilder::xmlName(jsval text, TokenPos *pos)
{
    return makeNode(AST_XMLNAME, pos, "contents", text);
}

JSObject *
NodeBuilder::xmlName(NodeList &elts, TokenPos *pos)
{
    return makeListNode(AST_XMLNAME, pos, "contents", elts);
}

JSObject *
NodeBuilder::xmlAttribute(jsval text, TokenPos *pos)
{
    return makeNode(AST_XMLATTR, pos, "value", text);
}

JSObject *
NodeBuilder::xmlCdata(jsval text, TokenPos *pos)
{
    return makeNode(AST_XMLCDATA, pos, "contents", text);
}

JSObject *
NodeBuilder::xmlComment(jsval text, TokenPos *pos)
{
    return makeNode(AST_XMLCOMMENT, pos, "contents", text);
}

JSObject *
NodeBuilder::xmlPI(jsval target, TokenPos *pos)
{
    return makeNode(AST_XMLPI, pos,
                    "target", target,
                    "contents", JSVAL_NULL);
}

JSObject *
NodeBuilder::xmlPI(jsval target, jsval contents, TokenPos *pos)
{
    return makeNode(AST_XMLPI, pos,
                    "target", target,
                    "contents", contents);
}


/*
 * Serialization of parse nodes to JavaScript objects.
 *
 * All serialization methods take a non-nullable JSParseNode pointer.
 */

class ASTSerializer
{
    JSContext     *cx;      /* ! */
    NodeBuilder   builder;  /* _ */

    /* (pn: ?) */
    JSObject *failAt(JSParseNode *pn) {
        if (!pn) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_PARSE_NODE,
                                 "unknown", "unknown location");
            return NULL;
        }

        static const size_t nbytes = 128;

        char buf1[nbytes];
        JS_snprintf(buf1, nbytes, "%d", pn->pn_type);

        char buf2[nbytes];
        JS_snprintf(buf2, nbytes, "%d:%d - %d:%d",
                    pn->pn_pos.begin.lineno, pn->pn_pos.begin.index,
                    pn->pn_pos.end.lineno, pn->pn_pos.end.index);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_PARSE_NODE, buf1, buf2);
        return NULL;
    }

    jsval atomContents(JSAtom *atom) {
        return STRING_TO_JSVAL(ATOM_TO_STRING(atom ? atom : cx->runtime->atomState.emptyAtom));
    }

    BinaryOperator binop(TokenKind tk, JSOp op);
    UnaryOperator unop(TokenKind tk, JSOp op);
    AssignmentOperator aop(JSOp op);

    bool statements(JSParseNode *pn, NodeList &elts);
    bool expressions(JSParseNode *pn, NodeList &elts);
    bool xmls(JSParseNode *pn, NodeList &elts);
    bool binaryOperands(JSParseNode *pn, NodeList &elts);
    bool functionArgs(JSParseNode *pn, JSParseNode *pnargs, JSParseNode *pndestruct,
                      JSParseNode *pnbody, NodeList &args);

    JSObject *sourceElement(JSParseNode *pn);
    JSObject *declaration(JSParseNode *pn);
    JSObject *variableDeclaration(JSParseNode *pn, bool let);
    JSObject *variableDeclarator(JSParseNode *pn, VarDeclKind *pkind);
    JSObject *statement(JSParseNode *pn);
    JSObject *blockStatement(JSParseNode *pn);
    JSObject *switchStatement(JSParseNode *pn);
    JSObject *switchCase(JSParseNode *pn);
    JSObject *tryStatement(JSParseNode *pn);
    JSObject *catchClause(JSParseNode *pn);
    JSObject *expression(JSParseNode *pn);
    JSObject *propertyName(JSParseNode *pn);
    JSObject *property(JSParseNode *pn);
    JSObject *literal(JSParseNode *pn);
    JSObject *identifier(JSAtom *atom, TokenPos *pos);
    JSObject *identifier(JSParseNode *pn);
    JSObject *pattern(JSParseNode *pn, VarDeclKind *pkind = NULL);
    JSObject *arrayPattern(JSParseNode *pn, VarDeclKind *pkind);
    JSObject *objectPattern(JSParseNode *pn, VarDeclKind *pkind);
    JSObject *function(JSParseNode *pn, ASTType type);
    JSObject *functionArgsAndBody(JSParseNode *pn, NodeList &args);
    JSObject *functionBody(JSParseNode *pn, TokenPos *pos);
    JSObject *comprehensionBlock(JSParseNode *pn);
    JSObject *comprehension(JSParseNode *pn);
    JSObject *generatorExpression(JSParseNode *pn);
    JSObject *xml(JSParseNode *pn);

  public:
    ASTSerializer(JSContext *c, char const *src)
        : cx(c), builder(c, src) {
    }

    JSObject *program(JSParseNode *pn);
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
ASTSerializer::statements(JSParseNode *pn, NodeList &elts)
{
    JS_ASSERT(pn->pn_type == TOK_LC && pn->pn_arity == PN_LIST);

    RFALSE_UNLESS(elts.reserve(pn->pn_count));

    for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
        JSObject *child = sourceElement(next);
        RFALSE_UNLESS(child);
        (void)elts.append(child); /* space check above */
    }

    return true;
}

bool
ASTSerializer::expressions(JSParseNode *pn, NodeList &elts)
{
    RFALSE_UNLESS(elts.reserve(pn->pn_count));

    for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
        JSObject *elt = expression(next);
        RFALSE_UNLESS(elt);
        (void)elts.append(elt); /* space check above */
    }

    return true;
}

bool
ASTSerializer::xmls(JSParseNode *pn, NodeList &elts)
{
    RFALSE_UNLESS(elts.reserve(pn->pn_count));

    for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
        JSObject *elt = xml(next);
        RFALSE_UNLESS(elt);
        (void)elts.append(elt); /* space check above */
    }

    return true;
}

JSObject *
ASTSerializer::blockStatement(JSParseNode *pn)
{
    JS_ASSERT(pn->pn_type == TOK_LC);

    NodeList list(cx);
    RNULL_UNLESS(statements(pn, list));
    return builder.blockStatement(list, &pn->pn_pos);
}

JSObject *
ASTSerializer::program(JSParseNode *pn)
{
    if (!pn) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_INTERNAL_ARG);
        return NULL;
    }

    NodeList list(cx);
    RNULL_UNLESS(statements(pn, list));
    return builder.program(list, &pn->pn_pos);
}

JSObject *
ASTSerializer::sourceElement(JSParseNode *pn)
{
    /* SpiderMonkey allows declarations even in pure statement contexts. */
    return statement(pn);
}

JSObject *
ASTSerializer::declaration(JSParseNode *pn)
{
    switch (pn->pn_type) {
      case TOK_FUNCTION:
        return function(pn, AST_FUNC_DECL);

      case TOK_VAR:
        return variableDeclaration(pn, false);

      case TOK_LET:
        return variableDeclaration(pn, true);

      default:
        return failAt(pn);
    }
}

JSObject *
ASTSerializer::variableDeclaration(JSParseNode *pn, bool let)
{
    JS_ASSERT(let ? pn->pn_type == TOK_LET : pn->pn_type == TOK_VAR);

    /* Later updated to VARDECL_CONST if we find a PND_CONST declarator. */
    VarDeclKind kind = let ? VARDECL_LET : VARDECL_VAR;

    NodeList list(cx);
    RNULL_UNLESS(list.reserve(pn->pn_count));

    /* In a for-in context, variable declarations contain just a single pattern. */
    if (pn->pn_xflags & PNX_FORINVAR) {
        JSObject *patt = pattern(pn->pn_head, &kind);
        RNULL_UNLESS(patt);
        JSObject *child = builder.variableDeclarator(patt, NULL, &pn->pn_head->pn_pos);
        RNULL_UNLESS(child);
        (void)list.append(child); /* space check above */
        return builder.variableDeclaration(list, kind, &pn->pn_pos);
    }

    for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
        JSObject *child = variableDeclarator(next, &kind);
        RNULL_UNLESS(child);
        (void)list.append(child); /* space check above */
    }

    return builder.variableDeclaration(list, kind, &pn->pn_pos);
}

JSObject *
ASTSerializer::variableDeclarator(JSParseNode *pn, VarDeclKind *pkind)
{
    /* A destructuring declarator is always a TOK_ASSIGN. */
    JS_ASSERT(pn->pn_type == TOK_NAME || pn->pn_type == TOK_ASSIGN);

    JSParseNode *pnleft;
    JSParseNode *pnright;

    switch (pn->pn_type) {
      case TOK_NAME:
        pnleft = pn;
        pnright = pn->pn_expr;
        break;

      case TOK_ASSIGN:
        pnleft = pn->pn_left;
        pnright = pn->pn_right;
        break;

      default:
        return failAt(pn);
    }

    JSObject *left = pattern(pnleft, pkind);
    RNULL_UNLESS(left);

    JSObject *right = NULL;
    if (pnright) {
        right = expression(pnright);
        RNULL_UNLESS(right);
    }

    return builder.variableDeclarator(left, right, &pn->pn_pos);
}

JSObject *
ASTSerializer::switchCase(JSParseNode *pn)
{
    JSObject *expr = NULL;
    if (pn->pn_left) {
        expr = expression(pn->pn_left);
        RNULL_UNLESS(expr);
    }

    NodeList list(cx);
    RNULL_UNLESS(statements(pn->pn_right, list));
    return builder.switchCase(expr, list, &pn->pn_pos);
}

JSObject *
ASTSerializer::switchStatement(JSParseNode *pn)
{
    JSObject *disc = expression(pn->pn_left);
    RNULL_UNLESS(disc);

    bool lexical;

    JSParseNode *listNode;

    if (pn->pn_right->pn_type == TOK_LEXICALSCOPE) {
        listNode = pn->pn_right->pn_expr;
        lexical = true;
    } else {
        listNode = pn->pn_right;
        lexical = false;
    }

    NodeList list(cx);
    RNULL_UNLESS(list.reserve(listNode->pn_count));

    for (JSParseNode *next = listNode->pn_head; next; next = next->pn_next) {
        JSObject *child = switchCase(next);
        RNULL_UNLESS(child);
        (void)list.append(child); /* space check above */
    }

    return builder.switchStatement(disc, list, lexical, &pn->pn_pos);
}

JSObject *
ASTSerializer::catchClause(JSParseNode *pn)
{
    JSObject *var = pattern(pn->pn_kid1);
    RNULL_UNLESS(var);

    JSObject *guard = NULL;
    if (pn->pn_kid2) {
        guard = expression(pn->pn_kid2);
        RNULL_UNLESS(guard);
    }

    JSObject *body = statement(pn->pn_kid3);
    RNULL_UNLESS(body);

    return builder.catchClause(var, guard, body, &pn->pn_pos);
}

JSObject *
ASTSerializer::tryStatement(JSParseNode *pn)
{
    JSObject *body = statement(pn->pn_kid1);
    RNULL_UNLESS(body);

    NodeList list(cx);
    if (pn->pn_kid2) {
        RNULL_UNLESS(list.reserve(pn->pn_kid2->pn_count));

        for (JSParseNode *next = pn->pn_kid2->pn_head; next; next = next->pn_next) {
            JSObject *clause = catchClause(next->pn_expr);
            RNULL_UNLESS(clause);
            (void)list.append(clause); /* space check above */
        }
    }

    JSObject *finally = NULL;
    if (pn->pn_kid3) {
        finally = statement(pn->pn_kid3);
        RNULL_UNLESS(finally);
    }

    return builder.tryStatement(body, list, finally, &pn->pn_pos);
}

JSObject *
ASTSerializer::statement(JSParseNode *pn)
{
    switch (pn->pn_type) {
      case TOK_FUNCTION:
      case TOK_VAR:
      case TOK_LET:
        return declaration(pn);

      case TOK_SEMI:
        if (pn->pn_kid) {
            JSObject *expr = expression(pn->pn_kid);
            RNULL_UNLESS(expr);
            return builder.expressionStatement(expr, &pn->pn_pos);
        }
        return builder.emptyStatement(&pn->pn_pos);

      case TOK_LEXICALSCOPE:
        pn = pn->pn_expr;
        if (pn->pn_type != TOK_LC)
            return statement(pn);
        /* FALL THROUGH */

      case TOK_LC:
        return blockStatement(pn);

      case TOK_IF:
      {
        JSObject *test = expression(pn->pn_kid1);
        RNULL_UNLESS(test);
        JSObject *cons = statement(pn->pn_kid2);
        RNULL_UNLESS(cons);
        JSObject *alt = NULL;
        if (pn->pn_kid3) {
            alt = statement(pn->pn_kid3);
            RNULL_UNLESS(alt);
        }
        return builder.ifStatement(test, cons, alt, &pn->pn_pos);
      }

      case TOK_SWITCH:
        return switchStatement(pn);

      case TOK_TRY:
        return tryStatement(pn);

      case TOK_WITH:
      case TOK_WHILE:
      {
        JSObject *expr = expression(pn->pn_left);
        RNULL_UNLESS(expr);
        JSObject *stmt = statement(pn->pn_right);
        RNULL_UNLESS(stmt);
        return pn->pn_type == TOK_WITH
               ? builder.withStatement(expr, stmt, &pn->pn_pos)
               : builder.whileStatement(expr, stmt, &pn->pn_pos);
      }

      case TOK_DO:
      {
        JSObject *stmt = statement(pn->pn_left);
        RNULL_UNLESS(stmt);
        JSObject *test = expression(pn->pn_right);
        RNULL_UNLESS(test);
        return builder.doWhileStatement(stmt, test, &pn->pn_pos);
      }

      case TOK_FOR:
      {
        JSParseNode *head = pn->pn_left;

        JSObject *stmt = statement(pn->pn_right);
        RNULL_UNLESS(stmt);

        bool isForEach = (pn->pn_iflags & JSITER_FOREACH) ? true : false;

        if (head->pn_type == TOK_IN) {
            JSObject *var = head->pn_left->pn_type == TOK_VAR
                            ? variableDeclaration(head->pn_left, false)
                            : head->pn_left->pn_type == TOK_LET
                            ? variableDeclaration(head->pn_left, true)
                            : pattern(head->pn_left);
            RNULL_UNLESS(var);

            JSObject *expr = expression(head->pn_right);
            RNULL_UNLESS(expr);

            return builder.forInStatement(var, expr, stmt, isForEach, &pn->pn_pos);
        }

        JSObject *init = NULL;
        if (head->pn_kid1) {
            init = (head->pn_kid1->pn_type == TOK_VAR)
                   ? variableDeclaration(head->pn_kid1, false)
                   : (head->pn_kid1->pn_type == TOK_LET)
                   ? variableDeclaration(head->pn_kid1, true)
                   : expression(head->pn_kid1);
            RNULL_UNLESS(init);
        }

        JSObject *test = NULL;
        if (head->pn_kid2) {
            test = expression(head->pn_kid2);
            RNULL_UNLESS(test);
        }

        JSObject *update = NULL;
        if (head->pn_kid3) {
            update = expression(head->pn_kid3);
            RNULL_UNLESS(update);
        }

        return builder.forStatement(init, test, update, stmt, &pn->pn_pos);
      }

      case TOK_BREAK:
      case TOK_CONTINUE:
      {
        JSObject *label = JSVAL_NULL;
        if (pn->pn_atom) {
            label = identifier(pn->pn_atom, NULL);
            RNULL_UNLESS(label);
        }

        return pn->pn_type == TOK_BREAK
               ? builder.breakStatement(label, &pn->pn_pos)
               : builder.continueStatement(label, &pn->pn_pos);
      }

      case TOK_COLON:
      {
        JSObject *label = identifier(pn->pn_atom, NULL);
        RNULL_UNLESS(label);

        JSObject *stmt = statement(pn->pn_expr);
        RNULL_UNLESS(stmt);

        return builder.labeledStatement(label, stmt, &pn->pn_pos);
      }

      case TOK_THROW:
      case TOK_RETURN:
      {
        JSObject *arg = NULL;
        if (pn->pn_kid) {
            arg = expression(pn->pn_kid);
            RNULL_UNLESS(arg);
        }
        return pn->pn_type == TOK_THROW
               ? builder.throwStatement(arg, &pn->pn_pos)
               : builder.returnStatement(arg, &pn->pn_pos);
      }

      case TOK_DEBUGGER:
        return builder.debuggerStatement(&pn->pn_pos);

#if JS_HAS_XML_SUPPORT
      case TOK_DEFAULT:
      {
        if (pn->pn_arity != PN_UNARY || pn->pn_kid->pn_type != TOK_STRING)
            return failAt(pn);

        JSObject *ns = literal(pn->pn_kid);
        RNULL_UNLESS(ns);
        return builder.xmlDefaultNamespace(ns, &pn->pn_pos);
      }
#endif

      default:
        return failAt(pn);
    }
}

bool
ASTSerializer::binaryOperands(JSParseNode *pn, NodeList &elts)
{
    if (pn->pn_arity == PN_BINARY) {
        RFALSE_UNLESS(elts.reserve(2));

        JSObject *left = expression(pn->pn_left);
        RFALSE_UNLESS(left);

        JSObject *right = expression(pn->pn_right);
        RFALSE_UNLESS(right);

        (void)elts.append(left);  /* space check above */
        (void)elts.append(right); /* space check above */

        return true;
    }

    JS_ASSERT(pn->pn_arity == PN_LIST);

    return expressions(pn, elts);
}

JSObject *
ASTSerializer::comprehensionBlock(JSParseNode *pn)
{
    JSParseNode *in = pn->pn_left;

    if (!in || in->pn_type != TOK_IN)
        return failAt(pn);

    bool isForEach = pn->pn_iflags & JSITER_FOREACH;

    JSObject *patt = pattern(in->pn_left);
    RNULL_UNLESS(patt);

    JSObject *src = expression(in->pn_right);
    RNULL_UNLESS(src);

    return builder.comprehensionBlock(patt, src, isForEach, &in->pn_pos);
}

JSObject *
ASTSerializer::comprehension(JSParseNode *pn)
{
    if (pn->pn_type != TOK_FOR)
        return failAt(pn);

    NodeList blocks(cx);

    JSParseNode *next = pn;
    while (next->pn_type == TOK_FOR) {
        JSObject *block = comprehensionBlock(next);
        RNULL_UNLESS(block);
        RNULL_UNLESS(blocks.append(block));
        next = next->pn_right;
    }

    JSObject *filter = NULL;

    if (next->pn_type == TOK_IF) {
        filter = expression(next->pn_kid1);
        RNULL_UNLESS(filter);
        next = next->pn_kid2;
    } else if (next->pn_type == TOK_LC && next->pn_count == 0) {
        /* js_FoldConstants optimized away the push. */
        NodeList empty(cx);
        return builder.arrayExpression(empty, &pn->pn_pos);
    }

    if (next->pn_type != TOK_ARRAYPUSH)
        return failAt(pn);

    JSObject *body = expression(next->pn_kid);
    RNULL_UNLESS(body);
    return builder.comprehensionExpression(body, blocks, filter, &pn->pn_pos);
}

JSObject *
ASTSerializer::generatorExpression(JSParseNode *pn)
{
    if (pn->pn_type != TOK_FOR)
        return failAt(pn);

    NodeList blocks(cx);

    JSParseNode *next = pn;
    while (next->pn_type == TOK_FOR) {
        JSObject *block = comprehensionBlock(next);
        RNULL_UNLESS(block);
        RNULL_UNLESS(blocks.append(block));
        next = next->pn_right;
    }

    JSObject *filter = NULL;

    if (next->pn_type == TOK_IF) {
        filter = expression(next->pn_kid1);
        RNULL_UNLESS(filter);
        next = next->pn_kid2;
    }

    if (next->pn_type != TOK_SEMI ||
        next->pn_kid->pn_type != TOK_YIELD ||
        !next->pn_kid->pn_kid)
        return failAt(pn);

    JSObject *body = expression(next->pn_kid->pn_kid);
    RNULL_UNLESS(body);

    return builder.generatorExpression(body, blocks, filter, &pn->pn_pos);
}

JSObject *
ASTSerializer::expression(JSParseNode *pn)
{
    switch (pn->pn_type) {
      case TOK_FUNCTION:
        return function(pn, AST_FUNC_EXPR);

      case TOK_COMMA:
      {
        NodeList list(cx);
        RNULL_UNLESS(expressions(pn, list));
        return builder.sequenceExpression(list, &pn->pn_pos);
      }

      case TOK_HOOK:
      {
        JSObject *test = expression(pn->pn_kid1);
        RNULL_UNLESS(test);

        JSObject *cons = expression(pn->pn_kid2);
        RNULL_UNLESS(cons);

        JSObject *alt = expression(pn->pn_kid3);
        RNULL_UNLESS(alt);

        return builder.conditionalExpression(test, cons, alt, &pn->pn_pos);
      }

      case TOK_OR:
      case TOK_AND:
      {
        NodeList operands(cx);
        RNULL_UNLESS(binaryOperands(pn, operands));
        return builder.logicalExpression(pn->pn_type == TOK_OR, operands, &pn->pn_pos);
      }

      case TOK_INC:
      case TOK_DEC:
      {
        JSObject *expr = expression(pn->pn_kid);
        RNULL_UNLESS(expr);

        bool incr = pn->pn_type == TOK_INC;
        bool prefix = pn->pn_op >= JSOP_INCNAME && pn->pn_op <= JSOP_DECELEM;

        return builder.updateExpression(expr, incr, prefix, &pn->pn_pos);
      }

      case TOK_ASSIGN:
      {
        JSObject *lhs = pattern(pn->pn_left);
        RNULL_UNLESS(lhs);

        JSObject *rhs = expression(pn->pn_right);
        RNULL_UNLESS(rhs);

        AssignmentOperator op = aop((JSOp)pn->pn_op);
        if (op == AOP_ERR)
            return failAt(pn);

        return builder.assignmentExpression(op, lhs, rhs, &pn->pn_pos);
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
      {
        NodeList operands(cx);
        RNULL_UNLESS(binaryOperands(pn, operands));

        BinaryOperator op = binop((TokenKind)pn->pn_type, (JSOp)pn->pn_op);
        if (op == BINOP_ERR)
            return failAt(pn);

        return builder.binaryExpression(op, operands, &pn->pn_pos);
      }

      case TOK_DELETE:
      case TOK_UNARYOP:
#if JS_HAS_XML_SUPPORT
        if (pn->pn_op == JSOP_XMLNAME)
            return expression(pn->pn_kid);
#endif

      {
        JSObject *expr = expression(pn->pn_kid);
        RNULL_UNLESS(expr);

        UnaryOperator op = unop((TokenKind)pn->pn_type, (JSOp)pn->pn_op);
        if (op == UNOP_ERR)
            return failAt(pn);

        return builder.unaryExpression(op, expr, &pn->pn_pos);
      }

      case TOK_NEW:
      case TOK_LP:
      {
#ifdef JS_HAS_GENERATOR_EXPRS
        if (pn->isGeneratorExpr())
            return generatorExpression(pn->generatorExpr());
#endif

        JSParseNode *next = pn->pn_head;

        JSObject *callee = expression(next);
        RNULL_UNLESS(callee);

        NodeList list(cx);
        RNULL_UNLESS(list.reserve(pn->pn_count - 1));

        for (next = next->pn_next; next; next = next->pn_next) {
            JSObject *arg = expression(next);
            RNULL_UNLESS(arg);
            (void)list.append(arg); /* space check above */
        }

        return pn->pn_type == TOK_NEW
               ? builder.newExpression(callee, list, &pn->pn_pos)
               : builder.callExpression(callee, list, &pn->pn_pos);
      }

      case TOK_DOT:
      {
        JSObject *expr = expression(pn->pn_expr);
        RNULL_UNLESS(expr);

        JSObject *id = identifier(pn->pn_atom, NULL);
        RNULL_UNLESS(id);

        return builder.memberExpression(false, expr, id, &pn->pn_pos);
      }

      case TOK_LB:
      {
        JSObject *left = expression(pn->pn_left);
        RNULL_UNLESS(left);

        JSObject *right = expression(pn->pn_right);
        RNULL_UNLESS(right);

        return builder.memberExpression(true, left, right, &pn->pn_pos);
      }

      case TOK_RB:
      {
        NodeList list(cx);
        RNULL_UNLESS(list.reserve(pn->pn_count));

        for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
            if (next->pn_type == TOK_COMMA) {
                (void)list.append(NULL); /* space check above */
            } else {
                JSObject *expr = expression(next);
                RNULL_UNLESS(expr);
                (void)list.append(expr); /* space check above */
            }
        }

        return builder.arrayExpression(list, &pn->pn_pos);
      }

      case TOK_RC:
      {
        NodeList list(cx);
        RNULL_UNLESS(list.reserve(pn->pn_count));

        for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
            JSObject *prop = property(next);
            RNULL_UNLESS(prop);
            (void)list.append(prop); /* space check above */
        }

        return builder.objectExpression(list, &pn->pn_pos);
      }

      case TOK_NAME:
        return identifier(pn);

      case TOK_STRING:
      case TOK_REGEXP:
      case TOK_NUMBER:
      case TOK_PRIMARY:
        return literal(pn);

      case TOK_YIELD:
      {
        JSObject *kid = NULL;
        if (pn->pn_kid) {
            kid = expression(pn->pn_kid);
            RNULL_UNLESS(kid);
        }
        return builder.yieldExpression(kid, &pn->pn_pos);
      }

      case TOK_DEFSHARP:
      {
        JSObject *kid = expression(pn->pn_kid);
        RNULL_UNLESS(kid);
        return builder.graphExpression(pn->pn_num, kid, &pn->pn_pos);
      }

      case TOK_USESHARP:
        return builder.graphIndexExpression(pn->pn_num, &pn->pn_pos);

      case TOK_ARRAYCOMP:
        if (pn->pn_count == 1) {
            if (pn->pn_head->pn_type != TOK_LEXICALSCOPE)
                return failAt(pn);

            return comprehension(pn->pn_head->pn_expr);
        }
        /* NOTE: it's no longer the case that pn_count could be 2. */
        return failAt(pn);

#ifdef JS_HAS_XML_SUPPORT
      case TOK_ANYNAME:
        return builder.xmlAnyName(&pn->pn_pos);

      case TOK_DBLCOLON:
      {
        JSObject *left;
        JSObject *right;
        bool computed;

        if (pn->pn_arity == PN_NAME) {
            left = expression(pn->pn_expr);
            RNULL_UNLESS(left);

            right = identifier(pn->pn_atom, NULL);
            RNULL_UNLESS(right);

            computed = false;
        } else if (pn->pn_arity == PN_BINARY) {
            left = expression(pn->pn_left);
            RNULL_UNLESS(left);

            right = expression(pn->pn_right);
            RNULL_UNLESS(right);

            computed = true;
        } else
            return failAt(pn);

        return builder.xmlQualifiedIdentifier(left, right, computed, &pn->pn_pos);
      }

      case TOK_AT:
      {
        JSObject *expr = expression(pn->pn_kid);
        RNULL_UNLESS(expr);

        return builder.xmlAttributeSelector(expr, &pn->pn_pos);
      }

      case TOK_FILTER:
      {
        JSObject *left = expression(pn->pn_left);
        RNULL_UNLESS(left);

        JSObject *right = expression(pn->pn_right);
        RNULL_UNLESS(right);

        return builder.xmlFilterExpression(left, right, &pn->pn_pos);
      }

      default:
        return xml(pn);
#else
      default:
        return failAt(pn);
#endif
    }
}

JSObject *
ASTSerializer::xml(JSParseNode *pn)
{
    switch (pn->pn_type) {
#ifdef JS_HAS_XML_SUPPORT
      case TOK_LC:
      {
        JSObject *expr = expression(pn->pn_kid);
        RNULL_UNLESS(expr);
        return builder.xmlEscapeExpression(expr, &pn->pn_pos);
      }

      case TOK_XMLELEM:
      case TOK_XMLLIST:
      case TOK_XMLSTAGO:
      case TOK_XMLETAGO:
      case TOK_XMLPTAGC:
      {
        NodeList list(cx);
        RNULL_UNLESS(xmls(pn, list));

        switch (pn->pn_type) {
          case TOK_XMLELEM:
            return builder.xmlElement(list, &pn->pn_pos);

          case TOK_XMLLIST:
            return builder.xmlList(list, &pn->pn_pos);

          case TOK_XMLSTAGO:
            return builder.xmlStartTag(list, &pn->pn_pos);

          case TOK_XMLETAGO:
            return builder.xmlEndTag(list, &pn->pn_pos);

          case TOK_XMLPTAGC:
            return builder.xmlPointTag(list, &pn->pn_pos);
        }
      }

      case TOK_XMLTEXT:
        return builder.xmlText(atomContents(pn->pn_atom), &pn->pn_pos);

      case TOK_XMLNAME:
        if (pn->pn_arity == PN_NULLARY)
            return builder.xmlName(atomContents(pn->pn_atom), &pn->pn_pos);

        if (pn->pn_arity == PN_LIST) {
            NodeList list(cx);
            RNULL_UNLESS(xmls(pn, list));
            return builder.xmlName(list, &pn->pn_pos);
        }

        return failAt(pn);

      case TOK_XMLATTR:
        return builder.xmlAttribute(atomContents(pn->pn_atom), &pn->pn_pos);

      case TOK_XMLCDATA:
        return builder.xmlCdata(atomContents(pn->pn_atom), &pn->pn_pos);

      case TOK_XMLCOMMENT:
        return builder.xmlComment(atomContents(pn->pn_atom), &pn->pn_pos);

      case TOK_XMLPI:
        if (!pn->pn_atom2)
            return builder.xmlPI(atomContents(pn->pn_atom), &pn->pn_pos);
        else
            return builder.xmlPI(atomContents(pn->pn_atom),
                                 atomContents(pn->pn_atom2),
                                 &pn->pn_pos);
#endif

      default:
        return failAt(pn);
    }
}

JSObject *
ASTSerializer::propertyName(JSParseNode *pn)
{
    switch (pn->pn_type) {
      case TOK_NAME:
        return identifier(pn);

      case TOK_STRING:
      case TOK_NUMBER:
        return literal(pn);

      default:
        return failAt(pn);
    }
}

JSObject *
ASTSerializer::property(JSParseNode *pn)
{
    PropKind kind;
    switch (pn->pn_op) {
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
        return failAt(pn);
    }

    JSObject *key = propertyName(pn->pn_left);
    RNULL_UNLESS(key);

    JSObject *val = expression(pn->pn_right);
    RNULL_UNLESS(val);

    return builder.propertyInitializer(key, val, kind, &pn->pn_pos);
}

JSObject *
ASTSerializer::literal(JSParseNode *pn)
{
    jsval val;
    switch (pn->pn_type) {
      case TOK_STRING:
        val = STRING_TO_JSVAL(ATOM_TO_STRING(pn->pn_atom));
        break;

      case TOK_REGEXP:
      {
        JSObject *re1 = pn->pn_objbox ? pn->pn_objbox->object : NULL;
        if (!re1 || !re1->isRegExp())
            return failAt(pn);

        JSObject *proto;
        RNULL_UNLESS(js_GetClassPrototype(cx, cx->fp->scopeChain, JSProto_RegExp, &proto));

        JSObject *re2 = js_CloneRegExpObject(cx, re1, proto);
        RNULL_UNLESS(re2);

        val = OBJECT_TO_JSVAL(re2);
        break;
      }

      case TOK_NUMBER:
        RNULL_UNLESS(JS_NewNumberValue(cx, pn->pn_dval, &val));
        break;

      case TOK_PRIMARY:
        val = pn->pn_op == JSOP_NULL ? JSVAL_NULL : BOOLEAN_TO_JSVAL(pn->pn_op == JSOP_TRUE);
        break;

      default:
        return failAt(pn);
    }

    return builder.literal(val, &pn->pn_pos);
}

JSObject *
ASTSerializer::arrayPattern(JSParseNode *pn, VarDeclKind *pkind)
{
    JS_ASSERT(pn->pn_type == TOK_RB);

    NodeList list(cx);

    RNULL_UNLESS(list.reserve(pn->pn_count));

    for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
        if (next->pn_type == TOK_COMMA)
            (void)list.append(NULL); /* space check above */
        else {
            JSObject *patt = pattern(next, pkind);
            RNULL_UNLESS(patt);
            (void)list.append(patt); /* space check above */
        }
    }

    return builder.arrayPattern(list, &pn->pn_pos);
}

JSObject *
ASTSerializer::objectPattern(JSParseNode *pn, VarDeclKind *pkind)
{
    JS_ASSERT(pn->pn_type == TOK_RC);

    NodeList list(cx);

    RNULL_UNLESS(list.reserve(pn->pn_count));

    for (JSParseNode *next = pn->pn_head; next; next = next->pn_next) {
        if (next->pn_op != JSOP_INITPROP)
            return failAt(pn);

        JSObject *key = propertyName(next->pn_left);
        RNULL_UNLESS(key);

        JSObject *patt = pattern(next->pn_right, pkind);
        RNULL_UNLESS(patt);

        JSObject *prop = builder.propertyPattern(key, patt, &next->pn_pos);
        RNULL_UNLESS(prop);

        (void)list.append(prop); /* space check above */
    }

    return builder.objectPattern(list, &pn->pn_pos);
}

JSObject *
ASTSerializer::pattern(JSParseNode *pn, VarDeclKind *pkind)
{
    switch (pn->pn_type) {
      case TOK_RC:
        return objectPattern(pn, pkind);

      case TOK_RB:
        return arrayPattern(pn, pkind);

      case TOK_NAME:
        if (pkind && (pn->pn_dflags & PND_CONST))
            *pkind = VARDECL_CONST;
        /* FALL THROUGH */

      default:
        return expression(pn);
    }
}

JSObject *
ASTSerializer::identifier(JSAtom *atom, TokenPos *pos)
{
    return builder.identifier(atomContents(atom), pos);
}

JSObject *
ASTSerializer::identifier(JSParseNode *pn)
{
    if ((pn->pn_arity != PN_NAME && pn->pn_arity != PN_NULLARY) || !pn->pn_atom)
        return failAt(pn);

    return identifier(pn->pn_atom, &pn->pn_pos);
}

JSObject *
ASTSerializer::function(JSParseNode *pn, ASTType type)
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

    JSObject *id = NULL;
    if (func->atom) {
        id = identifier(func->atom, NULL);
        RNULL_UNLESS(id);
    }

    NodeList args(cx);

    JSParseNode *argsAndBody = (pn->pn_body->pn_type == TOK_UPVARS)
                               ? pn->pn_body->pn_tree
                               : pn->pn_body;

    JSObject *body = functionArgsAndBody(argsAndBody, args);
    RNULL_UNLESS(body);

    return builder.function(type, &pn->pn_pos, id, args, body, isGenerator, isExpression);
}

JSObject *
ASTSerializer::functionArgsAndBody(JSParseNode *pn, NodeList &args)
{
    JSParseNode *pnargs;
    JSParseNode *pnbody;

    /* Extract the args and body separately. */
    if (pn->pn_type == TOK_ARGSBODY) {
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
        if (!head || head->pn_type != TOK_SEMI)
            return failAt(pnbody);

        pndestruct = head->pn_kid;
        if (!pndestruct || pndestruct->pn_type != TOK_COMMA)
            return failAt(pnbody);
    } else {
        pndestruct = NULL;
    }

    /* Serialize the arguments and body. */
    switch (pnbody->pn_type) {
      case TOK_RETURN: /* expression closure, no destructured args */
        RNULL_UNLESS(functionArgs(pn, pnargs, NULL, pnbody, args));
        return expression(pnbody->pn_kid);

      case TOK_SEQ:    /* expression closure with destructured args */
      {
        JSParseNode *pnstart = pnbody->pn_head->pn_next;
        if (!pnstart || pnstart->pn_type != TOK_RETURN)
            return failAt(pnbody);

        RNULL_UNLESS(functionArgs(pn, pnargs, pndestruct, pnbody, args));
        return expression(pnstart->pn_kid);
      }

      case TOK_LC:     /* statement closure */
      {
        JSParseNode *pnstart = (pnbody->pn_xflags & PNX_DESTRUCT)
                               ? pnbody->pn_head->pn_next
                               : pnbody->pn_head;
        RNULL_UNLESS(functionArgs(pn, pnargs, pndestruct, pnbody, args));
        return functionBody(pnstart, &pnbody->pn_pos);
      }

      default:
        return failAt(pnbody);
    }
}

bool
ASTSerializer::functionArgs(JSParseNode *pn, JSParseNode *pnargs, JSParseNode *pndestruct,
                            JSParseNode *pnbody, NodeList &args)
{
    uintN i = 0;
    JSParseNode *arg = pnargs ? pnargs->pn_head : NULL;
    JSParseNode *destruct = pndestruct ? pndestruct->pn_head : NULL;
    JSObject *node;

    /*
     * Arguments are found in potentially two different places: 1) the
     * argsbody sequence (which ends with the body node), or 2) a
     * destructuring initialization at the beginning of the body. Loop
     * |arg| through the argsbody and |destruct| through the initial
     * destructuring assignments, stopping only when we've exhausted
     * both.
     */
    while ((arg && arg != pnbody) || destruct) {
        if (arg && arg != pnbody && arg->frameSlot() == i) {
            node = identifier(arg);
            RFALSE_UNLESS(node);
            RFALSE_UNLESS(args.append(node));
            arg = arg->pn_next;
        } else if (destruct && destruct->pn_right->frameSlot() == i) {
            node = pattern(destruct->pn_left);
            RFALSE_UNLESS(node);
            RFALSE_UNLESS(args.append(node));
            destruct = destruct->pn_next;
        } else {
            (void)failAt(pn);
            return false;
        }
        ++i;
    }

    return true;
}

JSObject *
ASTSerializer::functionBody(JSParseNode *pn, TokenPos *pos)
{
    NodeList elts(cx);

    /* We aren't sure how many elements there are up front, so we'll check each append. */
    for (JSParseNode *next = pn; next; next = next->pn_next) {
        JSObject *child = sourceElement(next);
        RNULL_UNLESS(child);
        RNULL_UNLESS(elts.append(child));
    }

    return builder.blockStatement(elts, pos);
}

} /* namespace js */

/* Reflect class */

JSClass js_ASTNodeClass = {
    "ASTNode",
    JSCLASS_IS_ANONYMOUS | JSCLASS_HAS_CACHED_PROTO(JSProto_ASTNode),
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JSClass js_ReflectClass = {
    js_Reflect_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_Reflect),
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool
reflect_parse(JSContext *cx, uintN argc, jsval *vp)
{
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "Reflect.parse", "0", "s");
        return JS_FALSE;
    }

    JSString *src = js_ValueToString(cx, JS_ARGV(cx, vp)[0]);
    RFALSE_UNLESS(src);

    const char *filename = NULL;
    if (argc > 1) {
        JSString *str = js_ValueToString(cx, JS_ARGV(cx, vp)[1]);
        RFALSE_UNLESS(str);
        filename = JS_GetStringBytes(str);
    }

    Parser parser(cx);
    RFALSE_UNLESS(parser.init(JS_GetStringChars(src), JS_GetStringLength(src), NULL, filename, 1));

    JSParseNode *pn = parser.parse(NULL);
    RFALSE_UNLESS(pn);

    RFALSE_UNLESS(JS_EnterLocalRootScope(cx));

    ASTSerializer serialize(cx, filename);
    JSObject *obj = serialize.program(pn);
    if (!obj) {
        JS_LeaveLocalRootScopeWithResult(cx, JSVAL_NULL);
        return JS_FALSE;
    }

    jsval val = OBJECT_TO_JSVAL(obj);
    JS_LeaveLocalRootScopeWithResult(cx, val);
    JS_SET_RVAL(cx, vp, val);
    return JS_TRUE;
}

static JSFunctionSpec ast_methods[] = {
    JS_FS_END
};

static JSFunctionSpec reflect_static_methods[] = {
    JS_FN("parse", reflect_parse, 0, 0),
    JS_FS_END
};


JSObject *
js_InitReflectClasses(JSContext *cx, JSObject *obj)
{
    JSObject *Reflect;

    Reflect = JS_NewObject(cx, &js_ReflectClass, NULL, obj);
    if (!Reflect)
        return NULL;

    if (!JS_DefineProperty(cx, obj, js_Reflect_str, OBJECT_TO_JSVAL(Reflect),
                           JS_PropertyStub, JS_PropertyStub, 0))
        return NULL;

    if (!JS_DefineFunctions(cx, Reflect, reflect_static_methods))
        return NULL;

    if (!JS_InitClass(cx, obj, NULL, &js_ASTNodeClass, NULL, 0,
                      NULL, ast_methods, NULL, NULL))
        return NULL;

    return Reflect;
}
