/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A higher-order macro for enumerating keyword tokens. */

#ifndef Keywords_h_
#define Keywords_h_

#include "jsversion.h"

#if JS_HAS_CONST
#  define FOR_CONST_KEYWORD(macro) \
      macro(const, const_, TOK_CONST, JSOP_DEFCONST, JSVERSION_DEFAULT)
#else
#  define FOR_CONST_KEYWORD(macro) \
      macro(const, const_, TOK_RESERVED, JSOP_NOP, JSVERSION_DEFAULT)
#endif
#if JS_HAS_BLOCK_SCOPE
#  define FOR_LET_KEYWORD(macro) \
      macro(let, let, TOK_LET, JSOP_NOP, JSVERSION_1_7)
#else
#  define FOR_LET_KEYWORD(macro) \
      macro(let, let, TOK_STRICT_RESERVED, JSOP_NOP, JSVERSION_1_7)
#endif
#if JS_HAS_GENERATORS
#  define FOR_YIELD_KEYWORD(macro) \
      macro(yield, yield, TOK_YIELD, JSOP_NOP, JSVERSION_1_7)
#else
#  define FOR_YIELD_KEYWORD(macro) \
      macro(yield, yield, TOK_STRICT_RESERVED, JSOP_NOP, JSVERSION_1_7)
#endif

#define FOR_EACH_JAVASCRIPT_KEYWORD(macro) \
    macro(false, false_, TOK_FALSE, JSOP_FALSE, JSVERSION_DEFAULT) \
    macro(true, true_, TOK_TRUE, JSOP_TRUE, JSVERSION_DEFAULT) \
    macro(null, null, TOK_NULL, JSOP_NULL, JSVERSION_DEFAULT) \
    /* ES5 Keywords. */ \
    macro(break, break_, TOK_BREAK, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(case, case_, TOK_CASE, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(catch, catch_, TOK_CATCH, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(continue, continue_, TOK_CONTINUE, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(debugger, debugger, TOK_DEBUGGER, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(default, default_, TOK_DEFAULT, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(delete, delete_, TOK_DELETE, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(do, do_, TOK_DO, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(else, else_, TOK_ELSE, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(finally, finally_, TOK_FINALLY, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(for, for_, TOK_FOR, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(function, function, TOK_FUNCTION, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(if, if_, TOK_IF, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(in, in, TOK_IN, JSOP_IN, JSVERSION_DEFAULT) \
    macro(instanceof, instanceof, TOK_INSTANCEOF, JSOP_INSTANCEOF,JSVERSION_DEFAULT) \
    macro(new, new_, TOK_NEW, JSOP_NEW, JSVERSION_DEFAULT) \
    macro(return, return_, TOK_RETURN, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(switch, switch_, TOK_SWITCH, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(this, this_, TOK_THIS, JSOP_THIS, JSVERSION_DEFAULT) \
    macro(throw, throw_, TOK_THROW, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(try, try_, TOK_TRY, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(typeof, typeof, TOK_TYPEOF, JSOP_TYPEOF, JSVERSION_DEFAULT) \
    macro(var, var, TOK_VAR, JSOP_DEFVAR, JSVERSION_DEFAULT) \
    macro(void, void_, TOK_VOID, JSOP_VOID, JSVERSION_DEFAULT) \
    macro(while, while_, TOK_WHILE, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(with, with, TOK_WITH, JSOP_NOP, JSVERSION_DEFAULT) \
    /* ES5 reserved keywords reserved in all code. */ \
    macro(class, class_, TOK_RESERVED, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(enum, enum_, TOK_RESERVED, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(export, export, TOK_RESERVED, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(extends, extends, TOK_RESERVED, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(import, import, TOK_RESERVED, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(super, super, TOK_RESERVED, JSOP_NOP, JSVERSION_DEFAULT) \
    /* ES5 future reserved keywords in strict mode. */ \
    macro(implements, implements, TOK_STRICT_RESERVED, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(interface, interface, TOK_STRICT_RESERVED, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(package, package, TOK_STRICT_RESERVED, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(private, private_, TOK_STRICT_RESERVED, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(protected, protected_, TOK_STRICT_RESERVED, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(public, public_, TOK_STRICT_RESERVED, JSOP_NOP, JSVERSION_DEFAULT) \
    macro(static, static_, TOK_STRICT_RESERVED, JSOP_NOP, JSVERSION_DEFAULT) \
    /* Various conditional keywords. */ \
    FOR_CONST_KEYWORD(macro) \
    FOR_LET_KEYWORD(macro) \
    FOR_YIELD_KEYWORD(macro)

#endif /* Keywords_h_ */
