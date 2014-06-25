/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 667131;
var summary = 'yield ignored if maybeNoteLegacyGenerator called too late';
var actual = '';
var expect = '';

function testGenerator(f, desc) {
    reportCompare(f.isGenerator(), true, desc + ": is generator");
    reportCompare(typeof f(), "object", desc + ": calling doesn't crash");
}

function reported1() {
    (function(){})([yield[]], (""))
}

function reported2() {
    (function(){})(let(w = "") yield, (e))
}

function simplified1() {
    print([yield], (0))
}

function simplified2() {
    print(let(w) yield, (0))
}

function f1(a) { [x for (x in yield) for (y in (a))] }
function f2(a) { [x for (x in yield) if (y in (a))] }
function f3(a) { ([x for (x in yield) for (y in (a))]) }
function f4(a) { ([x for (x in yield) if (y in (a))]) }

function f7() { print({a:yield},(0)) }

function f8() { ([yield], (0)) }
function f9() { (let(w)yield, (0)) }

testGenerator(reported1, "reported function with array literal");
testGenerator(reported2, "reported function with let-expression");
testGenerator(simplified1, "reported function with array literal, simplified");
testGenerator(simplified2, "reported function with let-expression, simplified");
testGenerator(f1, "top-level array comprehension with paren expr in for-block");
testGenerator(f2, "top-level array comprehension with paren expr in if-block");
testGenerator(f3, "parenthesized array comprehension with paren expr in for-block");
testGenerator(f4, "parenthesized array comprehension with paren expr in if-block");
testGenerator(f7, "object literal");
testGenerator(f8, "array literal in paren exp");
testGenerator(f9, "let-expression in paren exp");
