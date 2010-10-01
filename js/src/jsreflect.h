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
#ifndef jsreflect_h___
#define jsreflect_h___

#include <stdlib.h>
#include "jspubtd.h"

namespace js {

enum ASTType {
    AST_ERROR = -1,
#define ASTDEF(ast, str) ast,
#include "jsast.tbl"
#undef ASTDEF
    AST_LIMIT
};

enum AssignmentOperator {
    AOP_ERR = -1,

    /* assign */
    AOP_ASSIGN = 0,
    /* operator-assign */
    AOP_PLUS, AOP_MINUS, AOP_STAR, AOP_DIV, AOP_MOD,
    /* shift-assign */
    AOP_LSH, AOP_RSH, AOP_URSH,
    /* binary */
    AOP_BITOR, AOP_BITXOR, AOP_BITAND,

    AOP_LIMIT
};

enum BinaryOperator {
    BINOP_ERR = -1,

    /* eq */
    BINOP_EQ = 0, BINOP_NE, BINOP_STRICTEQ, BINOP_STRICTNE,
    /* rel */
    BINOP_LT, BINOP_LE, BINOP_GT, BINOP_GE,
    /* shift */
    BINOP_LSH, BINOP_RSH, BINOP_URSH,
    /* arithmetic */
    BINOP_PLUS, BINOP_MINUS, BINOP_STAR, BINOP_DIV, BINOP_MOD,
    /* binary */
    BINOP_BITOR, BINOP_BITXOR, BINOP_BITAND,
    /* misc */
    BINOP_IN, BINOP_INSTANCEOF,
    /* xml */
    BINOP_DBLDOT,

    BINOP_LIMIT
};

enum UnaryOperator {
    UNOP_ERR = -1,

    UNOP_DELETE = 0,
    UNOP_NEG,
    UNOP_POS,
    UNOP_NOT,
    UNOP_BITNOT,
    UNOP_TYPEOF,
    UNOP_VOID,

    UNOP_LIMIT
};

enum VarDeclKind {
    VARDECL_ERR = -1,
    VARDECL_VAR = 0,
    VARDECL_CONST,
    VARDECL_LET,
    VARDECL_LET_HEAD,
    VARDECL_LIMIT
};

enum PropKind {
    PROP_ERR = -1,
    PROP_INIT = 0,
    PROP_GETTER,
    PROP_SETTER,
    PROP_LIMIT
};

extern char const *aopNames[];
extern char const *binopNames[];
extern char const *unopNames[];
extern char const *nodeTypeNames[];

} /* namespace js */

extern js::Class js_ReflectClass;

extern JSObject *
js_InitReflectClass(JSContext *cx, JSObject *obj);


#endif /* jsreflect_h___ */
