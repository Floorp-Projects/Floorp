/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A higher-order macro for enumerating reserved word tokens. */

#ifndef vm_ReservedWords_h
#define vm_ReservedWords_h

#define FOR_EACH_JAVASCRIPT_RESERVED_WORD(macro) \
    macro(false, false_, TokenKind::TOK_FALSE) \
    macro(true, true_, TokenKind::TOK_TRUE) \
    macro(null, null, TokenKind::TOK_NULL) \
    \
    /* Keywords. */ \
    macro(break, break_, TokenKind::TOK_BREAK) \
    macro(case, case_, TokenKind::TOK_CASE) \
    macro(catch, catch_, TokenKind::TOK_CATCH) \
    macro(const, const_, TokenKind::TOK_CONST) \
    macro(continue, continue_, TokenKind::TOK_CONTINUE) \
    macro(debugger, debugger, TokenKind::TOK_DEBUGGER) \
    macro(default, default_, TokenKind::TOK_DEFAULT) \
    macro(delete, delete_, TokenKind::TOK_DELETE) \
    macro(do, do_, TokenKind::TOK_DO) \
    macro(else, else_, TokenKind::TOK_ELSE) \
    macro(finally, finally_, TokenKind::TOK_FINALLY) \
    macro(for, for_, TokenKind::TOK_FOR) \
    macro(function, function, TokenKind::TOK_FUNCTION) \
    macro(if, if_, TokenKind::TOK_IF) \
    macro(in, in, TokenKind::TOK_IN) \
    macro(instanceof, instanceof, TokenKind::TOK_INSTANCEOF) \
    macro(new, new_, TokenKind::TOK_NEW) \
    macro(return, return_, TokenKind::TOK_RETURN) \
    macro(switch, switch_, TokenKind::TOK_SWITCH) \
    macro(this, this_, TokenKind::TOK_THIS) \
    macro(throw, throw_, TokenKind::TOK_THROW) \
    macro(try, try_, TokenKind::TOK_TRY) \
    macro(typeof, typeof_, TokenKind::TOK_TYPEOF) \
    macro(var, var, TokenKind::TOK_VAR) \
    macro(void, void_, TokenKind::TOK_VOID) \
    macro(while, while_, TokenKind::TOK_WHILE) \
    macro(with, with, TokenKind::TOK_WITH) \
    macro(import, import, TokenKind::TOK_IMPORT) \
    macro(export, export_, TokenKind::TOK_EXPORT) \
    macro(class, class_, TokenKind::TOK_CLASS) \
    macro(extends, extends, TokenKind::TOK_EXTENDS) \
    macro(super, super, TokenKind::TOK_SUPER) \
    \
    /* Future reserved words. */ \
    macro(enum, enum_, TokenKind::TOK_ENUM) \
    \
    /* Future reserved words, but only in strict mode. */ \
    macro(implements, implements, TokenKind::TOK_IMPLEMENTS) \
    macro(interface, interface, TokenKind::TOK_INTERFACE) \
    macro(package, package, TokenKind::TOK_PACKAGE) \
    macro(private, private_, TokenKind::TOK_PRIVATE) \
    macro(protected, protected_, TokenKind::TOK_PROTECTED) \
    macro(public, public_, TokenKind::TOK_PUBLIC) \
    \
    /* Contextual keywords. */ \
    macro(as, as, TokenKind::TOK_AS) \
    macro(async, async, TokenKind::TOK_ASYNC) \
    macro(await, await, TokenKind::TOK_AWAIT) \
    macro(from, from, TokenKind::TOK_FROM) \
    macro(get, get, TokenKind::TOK_GET) \
    macro(let, let, TokenKind::TOK_LET) \
    macro(of, of, TokenKind::TOK_OF) \
    macro(set, set, TokenKind::TOK_SET) \
    macro(static, static_, TokenKind::TOK_STATIC) \
    macro(target, target, TokenKind::TOK_TARGET) \
    /* \
     * Yield is a token inside function*.  Outside of a function*, it is a \
     * future reserved word in strict mode, but a keyword in JS1.7 even \
     * when strict.  Punt logic to parser. \
     */ \
    macro(yield, yield, TokenKind::TOK_YIELD)

#endif /* vm_ReservedWords_h */
