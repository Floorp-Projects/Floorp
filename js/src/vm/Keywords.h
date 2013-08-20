/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A higher-order macro for enumerating keyword tokens. */

#ifndef vm_Keywords_h
#define vm_Keywords_h

#include "jsversion.h"

#if JS_HAS_CONST
#  define FOR_CONST_KEYWORD(macro) \
      macro(const, const_, TOK_CONST, JSVERSION_DEFAULT)
#else
#  define FOR_CONST_KEYWORD(macro) \
      macro(const, const_, TOK_RESERVED, JSVERSION_DEFAULT)
#endif
#if JS_HAS_BLOCK_SCOPE
#  define FOR_LET_KEYWORD(macro) \
      macro(let, let, TOK_LET, JSVERSION_1_7)
#else
#  define FOR_LET_KEYWORD(macro) \
      macro(let, let, TOK_STRICT_RESERVED, JSVERSION_1_7)
#endif

#define FOR_EACH_JAVASCRIPT_KEYWORD(macro) \
    macro(false, false_, TOK_FALSE, JSVERSION_DEFAULT) \
    macro(true, true_, TOK_TRUE, JSVERSION_DEFAULT) \
    macro(null, null, TOK_NULL, JSVERSION_DEFAULT) \
    /* ES5 Keywords. */ \
    macro(break, break_, TOK_BREAK, JSVERSION_DEFAULT) \
    macro(case, case_, TOK_CASE, JSVERSION_DEFAULT) \
    macro(catch, catch_, TOK_CATCH, JSVERSION_DEFAULT) \
    macro(continue, continue_, TOK_CONTINUE, JSVERSION_DEFAULT) \
    macro(debugger, debugger, TOK_DEBUGGER, JSVERSION_DEFAULT) \
    macro(default, default_, TOK_DEFAULT, JSVERSION_DEFAULT) \
    macro(delete, delete_, TOK_DELETE, JSVERSION_DEFAULT) \
    macro(do, do_, TOK_DO, JSVERSION_DEFAULT) \
    macro(else, else_, TOK_ELSE, JSVERSION_DEFAULT) \
    macro(finally, finally_, TOK_FINALLY, JSVERSION_DEFAULT) \
    macro(for, for_, TOK_FOR, JSVERSION_DEFAULT) \
    macro(function, function, TOK_FUNCTION, JSVERSION_DEFAULT) \
    macro(if, if_, TOK_IF, JSVERSION_DEFAULT) \
    macro(in, in, TOK_IN, JSVERSION_DEFAULT) \
    macro(instanceof, instanceof, TOK_INSTANCEOF, JSVERSION_DEFAULT) \
    macro(new, new_, TOK_NEW, JSVERSION_DEFAULT) \
    macro(return, return_, TOK_RETURN, JSVERSION_DEFAULT) \
    macro(switch, switch_, TOK_SWITCH, JSVERSION_DEFAULT) \
    macro(this, this_, TOK_THIS, JSVERSION_DEFAULT) \
    macro(throw, throw_, TOK_THROW, JSVERSION_DEFAULT) \
    macro(try, try_, TOK_TRY, JSVERSION_DEFAULT) \
    macro(typeof, typeof, TOK_TYPEOF, JSVERSION_DEFAULT) \
    macro(var, var, TOK_VAR, JSVERSION_DEFAULT) \
    macro(void, void_, TOK_VOID, JSVERSION_DEFAULT) \
    macro(while, while_, TOK_WHILE, JSVERSION_DEFAULT) \
    macro(with, with, TOK_WITH, JSVERSION_DEFAULT) \
    /* ES5 reserved keywords reserved in all code. */ \
    macro(class, class_, TOK_RESERVED, JSVERSION_DEFAULT) \
    macro(enum, enum_, TOK_RESERVED, JSVERSION_DEFAULT) \
    macro(export, export, TOK_RESERVED, JSVERSION_DEFAULT) \
    macro(extends, extends, TOK_RESERVED, JSVERSION_DEFAULT) \
    macro(import, import, TOK_RESERVED, JSVERSION_DEFAULT) \
    macro(super, super, TOK_RESERVED, JSVERSION_DEFAULT) \
    /* ES5 future reserved keywords in strict mode. */ \
    macro(implements, implements, TOK_STRICT_RESERVED, JSVERSION_DEFAULT) \
    macro(interface, interface, TOK_STRICT_RESERVED, JSVERSION_DEFAULT) \
    macro(package, package, TOK_STRICT_RESERVED, JSVERSION_DEFAULT) \
    macro(private, private_, TOK_STRICT_RESERVED, JSVERSION_DEFAULT) \
    macro(protected, protected_, TOK_STRICT_RESERVED, JSVERSION_DEFAULT) \
    macro(public, public_, TOK_STRICT_RESERVED, JSVERSION_DEFAULT) \
    macro(static, static_, TOK_STRICT_RESERVED, JSVERSION_DEFAULT) \
    /* \
     * ES5 future reserved keyword in strict mode, keyword in JS1.7 even when \
     * not strict, keyword inside function* in all versions.  Punt logic to \
     * parser. \
     */ \
    macro(yield, yield, TOK_YIELD, JSVERSION_DEFAULT) \
    /* Various conditional keywords. */ \
    FOR_CONST_KEYWORD(macro) \
    FOR_LET_KEYWORD(macro)

#endif /* vm_Keywords_h */
