/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is JavaScript Engine testing utilities.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Dave Herman
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


//-----------------------------------------------------------------------------
var BUGNUMBER = 667131;
var summary = 'yield ignored if maybeNoteGenerator called too late';
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

function f5() { print(<a>{yield}</a>, (0)) }
function f6() { print(<>{yield}</>, (0)) }
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
testGenerator(f5, "xml literal");
testGenerator(f6, "xml list literal");
testGenerator(f7, "object literal");
testGenerator(f8, "array literal in paren exp");
testGenerator(f9, "let-expression in paren exp");
