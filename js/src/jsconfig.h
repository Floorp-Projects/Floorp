/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * JS configuration macros.
 */
#ifndef JS_VERSION
#define JS_VERSION 13
#endif

#if JS_VERSION == 10

#define JS_BUG_AUTO_INDEX_PROPS 1       /* new object o: o.p = v sets o[0]? */
#define JS_BUG_NULL_INDEX_PROPS 1       /* o[0] defaults to null, not void? */
#define JS_BUG_EMPTY_INDEX_ZERO 1       /* o[""] is equivalent to o[0]? */
#define JS_BUG_SHORT_CIRCUIT    1       /* 1 && 1 => true, 1 && 0 => 0 bug? */
#define JS_BUG_EAGER_TOSTRING   1       /* o.toString() trumps o.valueOf()? */
#define JS_BUG_VOID_TOSTRING    0       /* void 0 + 0 == "undefined0"? */
#define JS_BUG_EVAL_THIS_FUN    0       /* eval('this') in function f is f */
#define JS_BUG_EVAL_THIS_SCOPE  0       /* Math.eval('sin(x)') vs. local x */
#define JS_BUG_FALLIBLE_EQOPS   1       /* fallible/intransitive equality ops */
#define JS_BUG_FALLIBLE_TONUM   1       /* fallible ValueToNumber primitive */
#define JS_BUG_WITH_CLOSURE     0       /* with(o)function f(){} sets o.f? */

#define JS_HAS_PROP_DELETE      0       /* delete o.p removes p from o? */
#define JS_HAS_CALL_OBJECT      0       /* fun.caller is stack frame obj? */
#define JS_HAS_LABEL_STATEMENT  0       /* have break/continue to label:? */
#define JS_HAS_DO_WHILE_LOOP    0       /* have do {...} while (b)? */
#define JS_HAS_SWITCH_STATEMENT 0       /* have switch (v) {case c: ...}? */
#define JS_HAS_SOME_PERL_FUN    0       /* have array.join/reverse/sort? */
#define JS_HAS_MORE_PERL_FUN    0       /* have array.push, str.substr, etc? */
#define JS_HAS_VALUEOF_HINT     0       /* valueOf(hint) where hint is typeof */
#define JS_HAS_LEXICAL_CLOSURE  0       /* nested functions, lexically closed */
#define JS_HAS_APPLY_FUNCTION   0       /* have apply(fun, arg1, ... argN)? */
#define JS_HAS_OBJ_PROTO_PROP   0       /* have o.__proto__ etc.? */
#define JS_HAS_REGEXPS          0       /* have perl r.e.s via RegExp, /pat/ */
#define JS_HAS_SEQUENCE_OPS     0       /* have array.slice, string.concat? */
#define JS_HAS_OBJECT_LITERAL   0       /* have var o = {'foo': 42, 'bar':3}? */
#define JS_HAS_OBJ_WATCHPOINT   0       /* have o.watch and o.unwatch? */
#define JS_HAS_EXPORT_IMPORT    0       /* have export fun; import obj.fun? */
#define JS_HAS_EVAL_THIS_SCOPE  0       /* Math.eval is same as with (Math) */
#define JS_HAS_TRIPLE_EQOPS     0       /* have === and !== identity eqops? */
#define JS_HAS_SHARP_VARS       0       /* have #n=, #n# for object literals */
#define JS_HAS_REPLACE_LAMBDA   0       /* have string.replace(re, lambda)? */

#elif JS_VERSION == 11

#define JS_BUG_AUTO_INDEX_PROPS 0       /* new object o: o.p = v sets o[0]? */
#define JS_BUG_NULL_INDEX_PROPS 1       /* o[0] defaults to null, not void? */
#define JS_BUG_EMPTY_INDEX_ZERO 1       /* o[""] is equivalent to o[0]? */
#define JS_BUG_SHORT_CIRCUIT    1       /* 1 && 1 => true, 1 && 0 => 0 bug? */
#define JS_BUG_EAGER_TOSTRING   1       /* o.toString() trumps o.valueOf()? */
#define JS_BUG_VOID_TOSTRING    0       /* void 0 + 0 == "undefined0"? */
#define JS_BUG_EVAL_THIS_FUN    1       /* eval('this') in function f is f */
#define JS_BUG_EVAL_THIS_SCOPE  1       /* Math.eval('sin(x)') vs. local x */
#define JS_BUG_FALLIBLE_EQOPS   1       /* fallible/intransitive equality ops */
#define JS_BUG_FALLIBLE_TONUM   1       /* fallible ValueToNumber primitive */
#define JS_BUG_WITH_CLOSURE     0       /* with(o)function f(){} sets o.f? */

#define JS_HAS_PROP_DELETE      0       /* delete o.p removes p from o? */
#define JS_HAS_CALL_OBJECT      0       /* fun.caller is stack frame obj? */
#define JS_HAS_LABEL_STATEMENT  0       /* have break/continue to label:? */
#define JS_HAS_DO_WHILE_LOOP    0       /* have do {...} while (b)? */
#define JS_HAS_SWITCH_STATEMENT 0       /* have switch (v) {case c: ...}? */
#define JS_HAS_SOME_PERL_FUN    1       /* have array.join/reverse/sort? */
#define JS_HAS_MORE_PERL_FUN    0       /* have array.push, str.substr, etc? */
#define JS_HAS_VALUEOF_HINT     0       /* valueOf(hint) where hint is typeof */
#define JS_HAS_LEXICAL_CLOSURE  0       /* nested functions, lexically closed */
#define JS_HAS_APPLY_FUNCTION   0       /* have apply(fun, arg1, ... argN)? */
#define JS_HAS_OBJ_PROTO_PROP   0       /* have o.__proto__ etc.? */
#define JS_HAS_REGEXPS          0       /* have perl r.e.s via RegExp, /pat/ */
#define JS_HAS_SEQUENCE_OPS     0       /* have array.slice, string.concat? */
#define JS_HAS_OBJECT_LITERAL   0       /* have var o = {'foo': 42, 'bar':3}? */
#define JS_HAS_OBJ_WATCHPOINT   0       /* have o.watch and o.unwatch? */
#define JS_HAS_EXPORT_IMPORT    0       /* have export fun; import obj.fun? */
#define JS_HAS_EVAL_THIS_SCOPE  0       /* Math.eval is same as with (Math) */
#define JS_HAS_TRIPLE_EQOPS     0       /* have === and !== identity eqops? */
#define JS_HAS_SHARP_VARS       0       /* have #n=, #n# for object literals */
#define JS_HAS_REPLACE_LAMBDA   0       /* have string.replace(re, lambda)? */

#elif JS_VERSION == 12

#define JS_BUG_AUTO_INDEX_PROPS 0       /* new object o: o.p = v sets o[0]? */
#define JS_BUG_NULL_INDEX_PROPS 0       /* o[0] defaults to null, not void? */
#define JS_BUG_EMPTY_INDEX_ZERO 0       /* o[""] is equivalent to o[0]? */
#define JS_BUG_SHORT_CIRCUIT    0       /* 1 && 1 => true, 1 && 0 => 0 bug? */
#define JS_BUG_EAGER_TOSTRING   0       /* o.toString() trumps o.valueOf()? */
#define JS_BUG_VOID_TOSTRING    1       /* void 0 + 0 == "undefined0"? */
#define JS_BUG_EVAL_THIS_FUN    0       /* eval('this') in function f is f */
#define JS_BUG_EVAL_THIS_SCOPE  0       /* Math.eval('sin(x)') vs. local x */
#define JS_BUG_FALLIBLE_EQOPS   0       /* fallible/intransitive equality ops */
#define JS_BUG_FALLIBLE_TONUM   0       /* fallible ValueToNumber primitive */
#define JS_BUG_WITH_CLOSURE     1       /* with(o)function f(){} sets o.f? */

#define JS_HAS_PROP_DELETE      1       /* delete o.p removes p from o? */
#define JS_HAS_CALL_OBJECT      1       /* fun.caller is stack frame obj? */
#define JS_HAS_LABEL_STATEMENT  1       /* have break/continue to label:? */
#define JS_HAS_DO_WHILE_LOOP    1       /* have do {...} while (b)? */
#define JS_HAS_SWITCH_STATEMENT 1       /* have switch (v) {case c: ...}? */
#define JS_HAS_SOME_PERL_FUN    1       /* have array.join/reverse/sort? */
#define JS_HAS_MORE_PERL_FUN    1       /* have array.push, str.substr, etc? */
#define JS_HAS_VALUEOF_HINT     1       /* valueOf(hint) where hint is typeof */
#define JS_HAS_LEXICAL_CLOSURE  1       /* nested functions, lexically closed */
#define JS_HAS_APPLY_FUNCTION   1       /* have apply(fun, arg1, ... argN)? */
#define JS_HAS_OBJ_PROTO_PROP   1       /* have o.__proto__ etc.? */
#define JS_HAS_REGEXPS          1       /* have perl r.e.s via RegExp, /pat/ */
#define JS_HAS_SEQUENCE_OPS     1       /* have array.slice, string.concat? */
#define JS_HAS_OBJECT_LITERAL   1       /* have var o = {'foo': 42, 'bar':3}? */
#define JS_HAS_OBJ_WATCHPOINT   1       /* have o.watch and o.unwatch? */
#define JS_HAS_EXPORT_IMPORT    1       /* have export fun; import obj.fun? */
#define JS_HAS_EVAL_THIS_SCOPE  1       /* Math.eval is same as with (Math) */
#define JS_HAS_TRIPLE_EQOPS     0       /* have === and !== identity eqops? */
#define JS_HAS_SHARP_VARS       0       /* have #n=, #n# for object literals */
#define JS_HAS_REPLACE_LAMBDA   0       /* have string.replace(re, lambda)? */

#elif JS_VERSION == 13

#define JS_BUG_AUTO_INDEX_PROPS 0       /* new object o: o.p = v sets o[0]? */
#define JS_BUG_NULL_INDEX_PROPS 0       /* o[0] defaults to null, not void? */
#define JS_BUG_EMPTY_INDEX_ZERO 0       /* o[""] is equivalent to o[0]? */
#define JS_BUG_SHORT_CIRCUIT    0       /* 1 && 1 => true, 1 && 0 => 0 bug? */
#define JS_BUG_EAGER_TOSTRING   0       /* o.toString() trumps o.valueOf()? */
#define JS_BUG_VOID_TOSTRING    0       /* void 0 + 0 == "undefined0"? */
#define JS_BUG_EVAL_THIS_FUN    0       /* eval('this') in function f is f */
#define JS_BUG_EVAL_THIS_SCOPE  0       /* Math.eval('sin(x)') vs. local x */
#define JS_BUG_FALLIBLE_EQOPS   0       /* fallible/intransitive equality ops */
#define JS_BUG_FALLIBLE_TONUM   0       /* fallible ValueToNumber primitive */
#define JS_BUG_WITH_CLOSURE     1       /* with(o)function f(){} sets o.f? */

#define JS_HAS_PROP_DELETE      1       /* delete o.p removes p from o? */
#define JS_HAS_CALL_OBJECT      1       /* fun.caller is stack frame obj? */
#define JS_HAS_LABEL_STATEMENT  1       /* have break/continue to label:? */
#define JS_HAS_DO_WHILE_LOOP    1       /* have do {...} while (b)? */
#define JS_HAS_SWITCH_STATEMENT 1       /* have switch (v) {case c: ...}? */
#define JS_HAS_SOME_PERL_FUN    1       /* have array.join/reverse/sort? */
#define JS_HAS_MORE_PERL_FUN    1       /* have array.push, str.substr, etc? */
#define JS_HAS_VALUEOF_HINT     1       /* valueOf(hint) where hint is typeof */
#define JS_HAS_LEXICAL_CLOSURE  1       /* nested functions, lexically closed */
#define JS_HAS_APPLY_FUNCTION   1       /* have apply(fun, arg1, ... argN)? */
#define JS_HAS_OBJ_PROTO_PROP   1       /* have o.__proto__ etc.? */
#define JS_HAS_REGEXPS          1       /* have perl r.e.s via RegExp, /pat/ */
#define JS_HAS_SEQUENCE_OPS     1       /* have array.slice, string.concat? */
#define JS_HAS_OBJECT_LITERAL   1       /* have var o = {'foo': 42, 'bar':3}? */
#define JS_HAS_OBJ_WATCHPOINT   1       /* have o.watch and o.unwatch? */
#define JS_HAS_EXPORT_IMPORT    1       /* have export fun; import obj.fun? */
#define JS_HAS_EVAL_THIS_SCOPE  1       /* Math.eval is same as with (Math) */
#define JS_HAS_TRIPLE_EQOPS     1       /* have === and !== identity eqops? */
#define JS_HAS_SHARP_VARS       1       /* have #n=, #n# for object literals */
#define JS_HAS_REPLACE_LAMBDA   1       /* have string.replace(re, lambda)? */

#else

#error "unknown JS_VERSION"

#endif
