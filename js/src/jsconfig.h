/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
 * JS configuration macros.
 */
#ifndef JS_VERSION
#define JS_VERSION 150
#endif

#if JS_VERSION == 100

#define JS_BUG_AUTO_INDEX_PROPS 1       /* new object o: o.p = v sets o[0] */
#define JS_BUG_NULL_INDEX_PROPS 1       /* o[0] defaults to null, not void */
#define JS_BUG_EMPTY_INDEX_ZERO 1       /* o[""] is equivalent to o[0] */
#define JS_BUG_EAGER_TOSTRING   1       /* o.toString() trumps o.valueOf() */
#define JS_BUG_VOID_TOSTRING    0       /* void 0 + 0 == "undefined0" */
#define JS_BUG_EVAL_THIS_FUN    0       /* eval('this') in function f is f */
#define JS_BUG_EVAL_THIS_SCOPE  0       /* Math.eval('sin(x)') vs. local x */
#define JS_BUG_FALLIBLE_EQOPS   1       /* fallible/intransitive equality ops */
#define JS_BUG_FALLIBLE_TONUM   1       /* fallible ValueToNumber primitive */
#define JS_BUG_WITH_CLOSURE     0       /* with(o)function f(){} sets o.f */
#define JS_BUG_SET_ENUMERATE    1       /* o.p=q flags o.p JSPROP_ENUMERATE */

#define JS_HAS_PROP_DELETE      0       /* delete o.p removes p from o */
#define JS_HAS_CALL_OBJECT      0       /* fun.caller is stack frame obj */
#define JS_HAS_LABEL_STATEMENT  0       /* has break/continue to label: */
#define JS_HAS_DO_WHILE_LOOP    0       /* has do {...} while (b) */
#define JS_HAS_SWITCH_STATEMENT 0       /* has switch (v) {case c: ...} */
#define JS_HAS_SOME_PERL_FUN    0       /* has array.join/reverse/sort */
#define JS_HAS_MORE_PERL_FUN    0       /* has array.push, str.substr, etc */
#define JS_HAS_VALUEOF_HINT     0       /* valueOf(hint) where hint is typeof */
#define JS_HAS_LEXICAL_CLOSURE  0       /* nested functions, lexically closed */
#define JS_HAS_APPLY_FUNCTION   0       /* has fun.apply(obj, argArray) */
#define JS_HAS_CALL_FUNCTION    0       /* has fun.call(obj, arg1, ... argN) */
#define JS_HAS_OBJ_PROTO_PROP   0       /* has o.__proto__ etc. */
#define JS_HAS_REGEXPS          0       /* has perl r.e.s via RegExp, /pat/ */
#define JS_HAS_SEQUENCE_OPS     0       /* has array.slice, string.concat */
#define JS_HAS_INITIALIZERS     0       /* has var o = {'foo': 42, 'bar':3} */
#define JS_HAS_OBJ_WATCHPOINT   0       /* has o.watch and o.unwatch */
#define JS_HAS_EXPORT_IMPORT    0       /* has export fun; import obj.fun */
#define JS_HAS_EVAL_THIS_SCOPE  0       /* Math.eval is same as with (Math) */
#define JS_HAS_TRIPLE_EQOPS     0       /* has === and !== identity eqops */
#define JS_HAS_SHARP_VARS       0       /* has #n=, #n# for object literals */
#define JS_HAS_REPLACE_LAMBDA   0       /* has string.replace(re, lambda) */
#define JS_HAS_SCRIPT_OBJECT    0       /* has (new Script("x++")).exec() */
#define JS_HAS_XDR              0       /* has XDR API and object methods */
#define JS_HAS_EXCEPTIONS       0       /* has exception handling */
#define JS_HAS_UNDEFINED        0       /* has global "undefined" property */
#define JS_HAS_TOSOURCE         0       /* has Object/Array toSource method */
#define JS_HAS_IN_OPERATOR      0       /* has in operator ('p' in {p:1}) */
#define JS_HAS_INSTANCEOF       0       /* has {p:1} instanceof Object */
#define JS_HAS_ARGS_OBJECT      0       /* has minimal ECMA arguments object */
#define JS_HAS_DEBUGGER_KEYWORD 0       /* has hook for debugger keyword */
#define JS_HAS_ERROR_EXCEPTIONS 0       /* has error object hierarchy */
#define JS_HAS_CATCH_GUARD      0       /* has exception handling catch guard */
#define JS_HAS_NEW_OBJ_METHODS  0       /* has Object.prototype query methods */
#define JS_HAS_SPARSE_ARRAYS    0       /* array methods preserve empty elems */
#define JS_HAS_DFLT_MSG_STRINGS 1       /* provides English error messages */
#define JS_HAS_NUMBER_FORMATS   0       /* numbers have formatting methods */
#define JS_HAS_GETTER_SETTER    0       /* has JS2 getter/setter functions */
#define JS_HAS_UNEVAL           0       /* has uneval() top-level function */
#define JS_HAS_CONST            0       /* has JS2 const as alternative var */
#define JS_HAS_FUN_EXPR_STMT    0       /* has function expression statement */
#define JS_HAS_LVALUE_RETURN    0       /* has o.item(i) = j; for native item */

#elif JS_VERSION == 110

#define JS_BUG_AUTO_INDEX_PROPS 0       /* new object o: o.p = v sets o[0] */
#define JS_BUG_NULL_INDEX_PROPS 1       /* o[0] defaults to null, not void */
#define JS_BUG_EMPTY_INDEX_ZERO 1       /* o[""] is equivalent to o[0] */
#define JS_BUG_EAGER_TOSTRING   1       /* o.toString() trumps o.valueOf() */
#define JS_BUG_VOID_TOSTRING    0       /* void 0 + 0 == "undefined0" */
#define JS_BUG_EVAL_THIS_FUN    1       /* eval('this') in function f is f */
#define JS_BUG_EVAL_THIS_SCOPE  1       /* Math.eval('sin(x)') vs. local x */
#define JS_BUG_FALLIBLE_EQOPS   1       /* fallible/intransitive equality ops */
#define JS_BUG_FALLIBLE_TONUM   1       /* fallible ValueToNumber primitive */
#define JS_BUG_WITH_CLOSURE     0       /* with(o)function f(){} sets o.f */
#define JS_BUG_SET_ENUMERATE    1       /* o.p=q flags o.p JSPROP_ENUMERATE */

#define JS_HAS_PROP_DELETE      0       /* delete o.p removes p from o */
#define JS_HAS_CALL_OBJECT      0       /* fun.caller is stack frame obj */
#define JS_HAS_LABEL_STATEMENT  0       /* has break/continue to label: */
#define JS_HAS_DO_WHILE_LOOP    0       /* has do {...} while (b) */
#define JS_HAS_SWITCH_STATEMENT 0       /* has switch (v) {case c: ...} */
#define JS_HAS_SOME_PERL_FUN    1       /* has array.join/reverse/sort */
#define JS_HAS_MORE_PERL_FUN    0       /* has array.push, str.substr, etc */
#define JS_HAS_VALUEOF_HINT     0       /* valueOf(hint) where hint is typeof */
#define JS_HAS_LEXICAL_CLOSURE  0       /* nested functions, lexically closed */
#define JS_HAS_APPLY_FUNCTION   0       /* has apply(fun, arg1, ... argN) */
#define JS_HAS_CALL_FUNCTION    0       /* has fun.call(obj, arg1, ... argN) */
#define JS_HAS_OBJ_PROTO_PROP   0       /* has o.__proto__ etc. */
#define JS_HAS_REGEXPS          0       /* has perl r.e.s via RegExp, /pat/ */
#define JS_HAS_SEQUENCE_OPS     0       /* has array.slice, string.concat */
#define JS_HAS_INITIALIZERS     0       /* has var o = {'foo': 42, 'bar':3} */
#define JS_HAS_OBJ_WATCHPOINT   0       /* has o.watch and o.unwatch */
#define JS_HAS_EXPORT_IMPORT    0       /* has export fun; import obj.fun */
#define JS_HAS_EVAL_THIS_SCOPE  0       /* Math.eval is same as with (Math) */
#define JS_HAS_TRIPLE_EQOPS     0       /* has === and !== identity eqops */
#define JS_HAS_SHARP_VARS       0       /* has #n=, #n# for object literals */
#define JS_HAS_REPLACE_LAMBDA   0       /* has string.replace(re, lambda) */
#define JS_HAS_SCRIPT_OBJECT    0       /* has (new Script("x++")).exec() */
#define JS_HAS_XDR              0       /* has XDR API and object methods */
#define JS_HAS_EXCEPTIONS       0       /* has exception handling */
#define JS_HAS_UNDEFINED        0       /* has global "undefined" property */
#define JS_HAS_TOSOURCE         0       /* has Object/Array toSource method */
#define JS_HAS_IN_OPERATOR      0       /* has in operator ('p' in {p:1}) */
#define JS_HAS_INSTANCEOF       0       /* has {p:1} instanceof Object */
#define JS_HAS_ARGS_OBJECT      0       /* has minimal ECMA arguments object */
#define JS_HAS_DEBUGGER_KEYWORD 0       /* has hook for debugger keyword */
#define JS_HAS_ERROR_EXCEPTIONS 0       /* has error object hierarchy */
#define JS_HAS_CATCH_GUARD      0       /* has exception handling catch guard */
#define JS_HAS_NEW_OBJ_METHODS  0       /* has Object.prototype query methods */
#define JS_HAS_SPARSE_ARRAYS    0       /* array methods preserve empty elems */
#define JS_HAS_DFLT_MSG_STRINGS 1       /* provides English error messages */
#define JS_HAS_NUMBER_FORMATS   0       /* numbers have formatting methods */
#define JS_HAS_GETTER_SETTER    0       /* has JS2 getter/setter functions */
#define JS_HAS_UNEVAL           0       /* has uneval() top-level function */
#define JS_HAS_CONST            0       /* has JS2 const as alternative var */
#define JS_HAS_FUN_EXPR_STMT    0       /* has function expression statement */
#define JS_HAS_LVALUE_RETURN    0       /* has o.item(i) = j; for native item */

#elif JS_VERSION == 120

#define JS_BUG_AUTO_INDEX_PROPS 0       /* new object o: o.p = v sets o[0] */
#define JS_BUG_NULL_INDEX_PROPS 0       /* o[0] defaults to null, not void */
#define JS_BUG_EMPTY_INDEX_ZERO 0       /* o[""] is equivalent to o[0] */
#define JS_BUG_EAGER_TOSTRING   0       /* o.toString() trumps o.valueOf() */
#define JS_BUG_VOID_TOSTRING    1       /* void 0 + 0 == "undefined0" */
#define JS_BUG_EVAL_THIS_FUN    0       /* eval('this') in function f is f */
#define JS_BUG_EVAL_THIS_SCOPE  0       /* Math.eval('sin(x)') vs. local x */
#define JS_BUG_FALLIBLE_EQOPS   0       /* fallible/intransitive equality ops */
#define JS_BUG_FALLIBLE_TONUM   0       /* fallible ValueToNumber primitive */
#define JS_BUG_WITH_CLOSURE     1       /* with(o)function f(){} sets o.f */
#define JS_BUG_SET_ENUMERATE    1       /* o.p=q flags o.p JSPROP_ENUMERATE */

#define JS_HAS_PROP_DELETE      1       /* delete o.p removes p from o */
#define JS_HAS_CALL_OBJECT      1       /* fun.caller is stack frame obj */
#define JS_HAS_LABEL_STATEMENT  1       /* has break/continue to label: */
#define JS_HAS_DO_WHILE_LOOP    1       /* has do {...} while (b) */
#define JS_HAS_SWITCH_STATEMENT 1       /* has switch (v) {case c: ...} */
#define JS_HAS_SOME_PERL_FUN    1       /* has array.join/reverse/sort */
#define JS_HAS_MORE_PERL_FUN    1       /* has array.push, str.substr, etc */
#define JS_HAS_VALUEOF_HINT     1       /* valueOf(hint) where hint is typeof */
#define JS_HAS_LEXICAL_CLOSURE  1       /* nested functions, lexically closed */
#define JS_HAS_APPLY_FUNCTION   1       /* has apply(fun, arg1, ... argN) */
#define JS_HAS_CALL_FUNCTION    0       /* has fun.call(obj, arg1, ... argN) */
#define JS_HAS_OBJ_PROTO_PROP   1       /* has o.__proto__ etc. */
#define JS_HAS_REGEXPS          1       /* has perl r.e.s via RegExp, /pat/ */
#define JS_HAS_SEQUENCE_OPS     1       /* has array.slice, string.concat */
#define JS_HAS_INITIALIZERS     1       /* has var o = {'foo': 42, 'bar':3} */
#define JS_HAS_OBJ_WATCHPOINT   1       /* has o.watch and o.unwatch */
#define JS_HAS_EXPORT_IMPORT    1       /* has export fun; import obj.fun */
#define JS_HAS_EVAL_THIS_SCOPE  1       /* Math.eval is same as with (Math) */
#define JS_HAS_TRIPLE_EQOPS     0       /* has === and !== identity eqops */
#define JS_HAS_SHARP_VARS       0       /* has #n=, #n# for object literals */
#define JS_HAS_REPLACE_LAMBDA   0       /* has string.replace(re, lambda) */
#define JS_HAS_SCRIPT_OBJECT    0       /* has (new Script("x++")).exec() */
#define JS_HAS_XDR              0       /* has XDR API and object methods */
#define JS_HAS_EXCEPTIONS       0       /* has exception handling */
#define JS_HAS_UNDEFINED        0       /* has global "undefined" property */
#define JS_HAS_TOSOURCE         0       /* has Object/Array toSource method */
#define JS_HAS_IN_OPERATOR      0       /* has in operator ('p' in {p:1}) */
#define JS_HAS_INSTANCEOF       0       /* has {p:1} instanceof Object */
#define JS_HAS_ARGS_OBJECT      0       /* has minimal ECMA arguments object */
#define JS_HAS_DEBUGGER_KEYWORD 0       /* has hook for debugger keyword */
#define JS_HAS_ERROR_EXCEPTIONS 0       /* has error object hierarchy */
#define JS_HAS_CATCH_GUARD      0       /* has exception handling catch guard */
#define JS_HAS_NEW_OBJ_METHODS  0       /* has Object.prototype query methods */
#define JS_HAS_SPARSE_ARRAYS    0       /* array methods preserve empty elems */
#define JS_HAS_DFLT_MSG_STRINGS 1       /* provides English error messages */
#define JS_HAS_NUMBER_FORMATS   0       /* numbers have formatting methods */
#define JS_HAS_GETTER_SETTER    0       /* has JS2 getter/setter functions */
#define JS_HAS_UNEVAL           0       /* has uneval() top-level function */
#define JS_HAS_CONST            0       /* has JS2 const as alternative var */
#define JS_HAS_FUN_EXPR_STMT    0       /* has function expression statement */
#define JS_HAS_LVALUE_RETURN    0       /* has o.item(i) = j; for native item */

#elif JS_VERSION == 130

#define JS_BUG_AUTO_INDEX_PROPS 0       /* new object o: o.p = v sets o[0] */
#define JS_BUG_NULL_INDEX_PROPS 0       /* o[0] defaults to null, not void */
#define JS_BUG_EMPTY_INDEX_ZERO 0       /* o[""] is equivalent to o[0] */
#define JS_BUG_EAGER_TOSTRING   0       /* o.toString() trumps o.valueOf() */
#define JS_BUG_VOID_TOSTRING    0       /* void 0 + 0 == "undefined0" */
#define JS_BUG_EVAL_THIS_FUN    0       /* eval('this') in function f is f */
#define JS_BUG_EVAL_THIS_SCOPE  0       /* Math.eval('sin(x)') vs. local x */
#define JS_BUG_FALLIBLE_EQOPS   0       /* fallible/intransitive equality ops */
#define JS_BUG_FALLIBLE_TONUM   0       /* fallible ValueToNumber primitive */
#define JS_BUG_WITH_CLOSURE     1       /* with(o)function f(){} sets o.f */
#define JS_BUG_SET_ENUMERATE    0       /* o.p=q flags o.p JSPROP_ENUMERATE */

#define JS_HAS_PROP_DELETE      1       /* delete o.p removes p from o */
#define JS_HAS_CALL_OBJECT      1       /* fun.caller is stack frame obj */
#define JS_HAS_LABEL_STATEMENT  1       /* has break/continue to label: */
#define JS_HAS_DO_WHILE_LOOP    1       /* has do {...} while (b) */
#define JS_HAS_SWITCH_STATEMENT 1       /* has switch (v) {case c: ...} */
#define JS_HAS_SOME_PERL_FUN    1       /* has array.join/reverse/sort */
#define JS_HAS_MORE_PERL_FUN    1       /* has array.push, str.substr, etc */
#define JS_HAS_VALUEOF_HINT     1       /* valueOf(hint) where hint is typeof */
#define JS_HAS_LEXICAL_CLOSURE  1       /* nested functions, lexically closed */
#define JS_HAS_APPLY_FUNCTION   1       /* has apply(fun, arg1, ... argN) */
#define JS_HAS_CALL_FUNCTION    1       /* has fun.call(obj, arg1, ... argN) */
#define JS_HAS_OBJ_PROTO_PROP   1       /* has o.__proto__ etc. */
#define JS_HAS_REGEXPS          1       /* has perl r.e.s via RegExp, /pat/ */
#define JS_HAS_SEQUENCE_OPS     1       /* has array.slice, string.concat */
#define JS_HAS_INITIALIZERS     1       /* has var o = {'foo': 42, 'bar':3} */
#define JS_HAS_OBJ_WATCHPOINT   1       /* has o.watch and o.unwatch */
#define JS_HAS_EXPORT_IMPORT    1       /* has export fun; import obj.fun */
#define JS_HAS_EVAL_THIS_SCOPE  1       /* Math.eval is same as with (Math) */
#define JS_HAS_TRIPLE_EQOPS     1       /* has === and !== identity eqops */
#define JS_HAS_SHARP_VARS       1       /* has #n=, #n# for object literals */
#define JS_HAS_REPLACE_LAMBDA   1       /* has string.replace(re, lambda) */
#define JS_HAS_SCRIPT_OBJECT    1       /* has (new Script("x++")).exec() */
#define JS_HAS_XDR              1       /* has XDR API and object methods */
#define JS_HAS_EXCEPTIONS       0       /* has exception handling */
#define JS_HAS_UNDEFINED        1       /* has global "undefined" property */
#define JS_HAS_TOSOURCE         1       /* has Object/Array toSource method */
#define JS_HAS_IN_OPERATOR      0       /* has in operator ('p' in {p:1}) */
#define JS_HAS_INSTANCEOF       0       /* has {p:1} instanceof Object */
#define JS_HAS_ARGS_OBJECT      1       /* has minimal ECMA arguments object */
#define JS_HAS_DEBUGGER_KEYWORD 1       /* has hook for debugger keyword */
#define JS_HAS_ERROR_EXCEPTIONS 0       /* has error object hierarchy */
#define JS_HAS_CATCH_GUARD      0       /* has exception handling catch guard */
#define JS_HAS_NEW_OBJ_METHODS  0       /* has Object.prototype query methods */
#define JS_HAS_SPARSE_ARRAYS    0       /* array methods preserve empty elems */
#define JS_HAS_DFLT_MSG_STRINGS 1       /* provides English error messages */
#define JS_HAS_NUMBER_FORMATS   0       /* numbers have formatting methods */
#define JS_HAS_GETTER_SETTER    0       /* has JS2 getter/setter functions */
#define JS_HAS_UNEVAL           0       /* has uneval() top-level function */
#define JS_HAS_CONST            0       /* has JS2 const as alternative var */
#define JS_HAS_FUN_EXPR_STMT    0       /* has function expression statement */
#define JS_HAS_LVALUE_RETURN    0       /* has o.item(i) = j; for native item */

#elif JS_VERSION == 140

#define JS_BUG_AUTO_INDEX_PROPS 0       /* new object o: o.p = v sets o[0] */
#define JS_BUG_NULL_INDEX_PROPS 0       /* o[0] defaults to null, not void */
#define JS_BUG_EMPTY_INDEX_ZERO 0       /* o[""] is equivalent to o[0] */
#define JS_BUG_EAGER_TOSTRING   0       /* o.toString() trumps o.valueOf() */
#define JS_BUG_VOID_TOSTRING    0       /* void 0 + 0 == "undefined0" */
#define JS_BUG_EVAL_THIS_FUN    0       /* eval('this') in function f is f */
#define JS_BUG_EVAL_THIS_SCOPE  0       /* Math.eval('sin(x)') vs. local x */
#define JS_BUG_FALLIBLE_EQOPS   0       /* fallible/intransitive equality ops */
#define JS_BUG_FALLIBLE_TONUM   0       /* fallible ValueToNumber primitive */
#define JS_BUG_WITH_CLOSURE     1       /* with(o)function f(){} sets o.f */
#define JS_BUG_SET_ENUMERATE    0       /* o.p=q flags o.p JSPROP_ENUMERATE */

#define JS_HAS_PROP_DELETE      1       /* delete o.p removes p from o */
#define JS_HAS_CALL_OBJECT      1       /* fun.caller is stack frame obj */
#define JS_HAS_LABEL_STATEMENT  1       /* has break/continue to label: */
#define JS_HAS_DO_WHILE_LOOP    1       /* has do {...} while (b) */
#define JS_HAS_SWITCH_STATEMENT 1       /* has switch (v) {case c: ...} */
#define JS_HAS_SOME_PERL_FUN    1       /* has array.join/reverse/sort */
#define JS_HAS_MORE_PERL_FUN    1       /* has array.push, str.substr, etc */
#define JS_HAS_VALUEOF_HINT     1       /* valueOf(hint) where hint is typeof */
#define JS_HAS_LEXICAL_CLOSURE  1       /* nested functions, lexically closed */
#define JS_HAS_APPLY_FUNCTION   1       /* has apply(fun, arg1, ... argN) */
#define JS_HAS_CALL_FUNCTION    1       /* has fun.call(obj, arg1, ... argN) */
#define JS_HAS_OBJ_PROTO_PROP   1       /* has o.__proto__ etc. */
#define JS_HAS_REGEXPS          1       /* has perl r.e.s via RegExp, /pat/ */
#define JS_HAS_SEQUENCE_OPS     1       /* has array.slice, string.concat */
#define JS_HAS_INITIALIZERS     1       /* has var o = {'foo': 42, 'bar':3} */
#define JS_HAS_OBJ_WATCHPOINT   1       /* has o.watch and o.unwatch */
#define JS_HAS_EXPORT_IMPORT    1       /* has export fun; import obj.fun */
#define JS_HAS_EVAL_THIS_SCOPE  1       /* Math.eval is same as with (Math) */
#define JS_HAS_TRIPLE_EQOPS     1       /* has === and !== identity eqops */
#define JS_HAS_SHARP_VARS       1       /* has #n=, #n# for object literals */
#define JS_HAS_REPLACE_LAMBDA   1       /* has string.replace(re, lambda) */
#define JS_HAS_SCRIPT_OBJECT    1       /* has (new Script("x++")).exec() */
#define JS_HAS_XDR		1	/* has XDR API and object methods */
#define JS_HAS_EXCEPTIONS	1	/* has exception handling */
#define JS_HAS_UNDEFINED        1       /* has global "undefined" property */
#define JS_HAS_TOSOURCE         1       /* has Object/Array toSource method */
#define JS_HAS_IN_OPERATOR      1       /* has in operator ('p' in {p:1}) */
#define JS_HAS_INSTANCEOF       1       /* has {p:1} instanceof Object */
#define JS_HAS_ARGS_OBJECT      1       /* has minimal ECMA arguments object */
#define JS_HAS_DEBUGGER_KEYWORD 1       /* has hook for debugger keyword */
#define JS_HAS_ERROR_EXCEPTIONS 0       /* rt errors reflected as exceptions */
#define JS_HAS_CATCH_GUARD      0       /* has exception handling catch guard */
#define JS_HAS_NEW_OBJ_METHODS  0       /* has Object.prototype query methods */
#define JS_HAS_SPARSE_ARRAYS    0       /* array methods preserve empty elems */
#define JS_HAS_DFLT_MSG_STRINGS 1       /* provides English error messages */
#define JS_HAS_NUMBER_FORMATS   0       /* numbers have formatting methods */
#define JS_HAS_GETTER_SETTER    0       /* has JS2 getter/setter functions */
#define JS_HAS_UNEVAL           0       /* has uneval() top-level function */
#define JS_HAS_CONST            0       /* has JS2 const as alternative var */
#define JS_HAS_FUN_EXPR_STMT    0       /* has function expression statement */
#define JS_HAS_LVALUE_RETURN    0       /* has o.item(i) = j; for native item */

#elif JS_VERSION == 150

#define JS_BUG_AUTO_INDEX_PROPS 0       /* new object o: o.p = v sets o[0] */
#define JS_BUG_NULL_INDEX_PROPS 0       /* o[0] defaults to null, not void */
#define JS_BUG_EMPTY_INDEX_ZERO 0       /* o[""] is equivalent to o[0] */
#define JS_BUG_EAGER_TOSTRING   0       /* o.toString() trumps o.valueOf() */
#define JS_BUG_VOID_TOSTRING    0       /* void 0 + 0 == "undefined0" */
#define JS_BUG_EVAL_THIS_FUN    0       /* eval('this') in function f is f */
#define JS_BUG_EVAL_THIS_SCOPE  0       /* Math.eval('sin(x)') vs. local x */
#define JS_BUG_FALLIBLE_EQOPS   0       /* fallible/intransitive equality ops */
#define JS_BUG_FALLIBLE_TONUM   0       /* fallible ValueToNumber primitive */
#define JS_BUG_WITH_CLOSURE     0       /* with(o)function f(){} sets o.f */
#define JS_BUG_SET_ENUMERATE    0       /* o.p=q flags o.p JSPROP_ENUMERATE */

#define JS_HAS_PROP_DELETE      1       /* delete o.p removes p from o */
#define JS_HAS_CALL_OBJECT      1       /* fun.caller is stack frame obj */
#define JS_HAS_LABEL_STATEMENT  1       /* has break/continue to label: */
#define JS_HAS_DO_WHILE_LOOP    1       /* has do {...} while (b) */
#define JS_HAS_SWITCH_STATEMENT 1       /* has switch (v) {case c: ...} */
#define JS_HAS_SOME_PERL_FUN    1       /* has array.join/reverse/sort */
#define JS_HAS_MORE_PERL_FUN    1       /* has array.push, str.substr, etc */
#define JS_HAS_VALUEOF_HINT     1       /* valueOf(hint) where hint is typeof */
#define JS_HAS_LEXICAL_CLOSURE  1       /* nested functions, lexically closed */
#define JS_HAS_APPLY_FUNCTION   1       /* has apply(fun, arg1, ... argN) */
#define JS_HAS_CALL_FUNCTION    1       /* has fun.call(obj, arg1, ... argN) */
#define JS_HAS_OBJ_PROTO_PROP   1       /* has o.__proto__ etc. */
#define JS_HAS_REGEXPS          1       /* has perl r.e.s via RegExp, /pat/ */
#define JS_HAS_SEQUENCE_OPS     1       /* has array.slice, string.concat */
#define JS_HAS_INITIALIZERS     1       /* has var o = {'foo': 42, 'bar':3} */
#define JS_HAS_OBJ_WATCHPOINT   1       /* has o.watch and o.unwatch */
#define JS_HAS_EXPORT_IMPORT    1       /* has export fun; import obj.fun */
#define JS_HAS_EVAL_THIS_SCOPE  1       /* Math.eval is same as with (Math) */
#define JS_HAS_TRIPLE_EQOPS     1       /* has === and !== identity eqops */
#define JS_HAS_SHARP_VARS       1       /* has #n=, #n# for object literals */
#define JS_HAS_REPLACE_LAMBDA   1       /* has string.replace(re, lambda) */
#define JS_HAS_SCRIPT_OBJECT    1       /* has (new Script("x++")).exec() */
#define JS_HAS_XDR              1       /* has XDR API and object methods */
#define JS_HAS_EXCEPTIONS       1       /* has exception handling */
#define JS_HAS_UNDEFINED        1       /* has global "undefined" property */
#define JS_HAS_TOSOURCE         1       /* has Object/Array toSource method */
#define JS_HAS_IN_OPERATOR      1       /* has in operator ('p' in {p:1}) */
#define JS_HAS_INSTANCEOF       1       /* has {p:1} instanceof Object */
#define JS_HAS_ARGS_OBJECT      1       /* has minimal ECMA arguments object */
#define JS_HAS_DEBUGGER_KEYWORD 1       /* has hook for debugger keyword */
#define JS_HAS_ERROR_EXCEPTIONS 1       /* rt errors reflected as exceptions */
#define JS_HAS_CATCH_GUARD      1       /* has exception handling catch guard */
#define JS_HAS_NEW_OBJ_METHODS  1       /* has Object.prototype query methods */
#define JS_HAS_SPARSE_ARRAYS    0       /* array methods preserve empty elems */
#define JS_HAS_DFLT_MSG_STRINGS 1       /* provides English error messages */
#define JS_HAS_NUMBER_FORMATS   1       /* numbers have formatting methods */
#define JS_HAS_GETTER_SETTER    1       /* has JS2 getter/setter functions */
#define JS_HAS_UNEVAL           1       /* has uneval() top-level function */
#define JS_HAS_CONST            1       /* has JS2 const as alternative var */
#define JS_HAS_FUN_EXPR_STMT    1       /* has function expression statement */
#define JS_HAS_LVALUE_RETURN    1       /* has o.item(i) = j; for native item */

#else

#error "unknown JS_VERSION"

#endif
