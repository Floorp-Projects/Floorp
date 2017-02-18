/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A higher-order macro for enumerating reserved word tokens. */

#ifndef vm_ReservedWords_h
#define vm_ReservedWords_h

#define FOR_EACH_JAVASCRIPT_RESERVED_WORD(macro) \
    macro(false, false_, TOK_FALSE) \
    macro(true, true_, TOK_TRUE) \
    macro(null, null, TOK_NULL) \
    \
    /* Keywords. */ \
    macro(break, break_, TOK_BREAK) \
    macro(case, case_, TOK_CASE) \
    macro(catch, catch_, TOK_CATCH) \
    macro(const, const_, TOK_CONST) \
    macro(continue, continue_, TOK_CONTINUE) \
    macro(debugger, debugger, TOK_DEBUGGER) \
    macro(default, default_, TOK_DEFAULT) \
    macro(delete, delete_, TOK_DELETE) \
    macro(do, do_, TOK_DO) \
    macro(else, else_, TOK_ELSE) \
    macro(finally, finally_, TOK_FINALLY) \
    macro(for, for_, TOK_FOR) \
    macro(function, function, TOK_FUNCTION) \
    macro(if, if_, TOK_IF) \
    macro(in, in, TOK_IN) \
    macro(instanceof, instanceof, TOK_INSTANCEOF) \
    macro(new, new_, TOK_NEW) \
    macro(return, return_, TOK_RETURN) \
    macro(switch, switch_, TOK_SWITCH) \
    macro(this, this_, TOK_THIS) \
    macro(throw, throw_, TOK_THROW) \
    macro(try, try_, TOK_TRY) \
    macro(typeof, typeof_, TOK_TYPEOF) \
    macro(var, var, TOK_VAR) \
    macro(void, void_, TOK_VOID) \
    macro(while, while_, TOK_WHILE) \
    macro(with, with, TOK_WITH) \
    macro(import, import, TOK_IMPORT) \
    macro(export, export_, TOK_EXPORT) \
    macro(class, class_, TOK_CLASS) \
    macro(extends, extends, TOK_EXTENDS) \
    macro(super, super, TOK_SUPER) \
    \
    /* Future reserved words. */ \
    macro(enum, enum_, TOK_ENUM) \
    \
    /* Future reserved words, but only in strict mode. */ \
    macro(implements, implements, TOK_IMPLEMENTS) \
    macro(interface, interface, TOK_INTERFACE) \
    macro(package, package, TOK_PACKAGE) \
    macro(private, private_, TOK_PRIVATE) \
    macro(protected, protected_, TOK_PROTECTED) \
    macro(public, public_, TOK_PUBLIC) \
    \
    /* Contextual keywords. */ \
    macro(as, as, TOK_AS) \
    macro(async, async, TOK_ASYNC) \
    macro(await, await, TOK_AWAIT) \
    macro(each, each, TOK_EACH) \
    macro(from, from, TOK_FROM) \
    macro(get, get, TOK_GET) \
    macro(let, let, TOK_LET) \
    macro(of, of, TOK_OF) \
    macro(set, set, TOK_SET) \
    macro(static, static_, TOK_STATIC) \
    macro(target, target, TOK_TARGET) \
    /* \
     * Yield is a token inside function*.  Outside of a function*, it is a \
     * future reserved word in strict mode, but a keyword in JS1.7 even \
     * when strict.  Punt logic to parser. \
     */ \
    macro(yield, yield, TOK_YIELD)

#endif /* vm_ReservedWords_h */
