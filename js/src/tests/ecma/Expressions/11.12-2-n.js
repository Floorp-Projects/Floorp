/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.12-2-n.js
   ECMA Section:       11.12
   Description:

   The grammar for a ConditionalExpression in ECMAScript is a little bit
   different from that in C and Java, which each allow the second
   subexpression to be an Expression but restrict the third expression to
   be a ConditionalExpression.  The motivation for this difference in
   ECMAScript is to allow an assignment expression to be governed by either
   arm of a conditional and to eliminate the confusing and fairly useless
   case of a comma expression as the center expression.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "11.12-2-n";
writeHeaderToLog( SECTION + " Conditional operator ( ? : )");

// the following expression should be an error in JS.

DESCRIPTION = "var MYVAR =  true ? 'EXPR1', 'EXPR2' : 'EXPR3'; MYVAR";

new TestCase( "var MYVAR =  true ? 'EXPR1', 'EXPR2' : 'EXPR3'; MYVAR",
              "error",
              eval("var MYVAR =  true ? 'EXPR1', 'EXPR2' : 'EXPR3'; MYVAR") );

test();

