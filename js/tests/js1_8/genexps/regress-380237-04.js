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
 * Contributor(s): Jesse Ruderman
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

var gTestfile = 'regress-380237-04.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 380237;
var summary = 'Generator expressions parenthesization test';
var actual = '';
var expect = '';


/*

Given that parentheization seems so fragile *and* the rules for where
genexps are allowed keep changing, I thought it would be good to have
a way to test that:

1) unparenthesized genexps are allowed in some places and the
decompilation is sane and not over-parenthesized

2) unparenthesized genexps are disallowed in many places and when
there are parens, the decompilation is sane and not over-parenthesized

*/

// |genexp| must have the exact same whitespace the decompiler uses
genexp = "x * x for (x in [])";
genexpParened = "(" + genexp + ")";
genexpParenedTwice = "(" + genexpParened + ")";

// Warning: be careful not to put [] around stuff, because that would
// cause it to be treated as an array comprehension instead of a
// generator expression!

// Statements
doesNotNeedParens("if (xx) { }");
needParens("if (1, xx) { }");
needParens("if (xx, 1) { }");
doesNotNeedParens("do { } while (xx);");
doesNotNeedParens("while (xx) { }");
doesNotNeedParens("switch (xx) { }");
doesNotNeedParens("with (xx) { }");
needParens("switch (x) { case xx: }");
needParens("return xx;");
needParens("yield xx;");
needParens("for (xx;;) { }");
needParens("for (;xx;) { }");
needParens("for (;;xx) { }");
needParens("for (i in xx) { }");
needParens("throw xx");
needParens("try { } catch (e if xx) { }");
needParens("let (x=3) xx");
needParens("let (x=xx) 3");

// Function calls
doesNotNeedParens("f(xx);");
needParens("f(xx, 1);");
needParens("f(1, xx);");
doesNotNeedParens("/x/(xx);");
needParens("/x/(xx, 1);");
needParens("/x/(1, xx);");

// eval is special and often confuses the decompiler.
doesNotNeedParens("eval(xx);");
needParens("eval(xx, 1);");
needParens("eval(1, xx);");

// Expressions
needParens("xx;");            // ???
needParens("var g = xx;");    // ???
needParens("g += xx;");
needParens("xx();");
needParens("xx() = 3;");
needParens("a ? xx : c");
needParens("xx ? b : c");
needParens("a ? b : xx");
needParens("1 ? xx : c");
needParens("0 ? b : xx");
needParens("1 + xx");
needParens("xx + 1");
needParens("1, xx");
doesNotNeedParens("+(xx)");
doesNotNeedParens("!(xx)");
needParens("xx, 1");
needParens("[1, xx]");
needParens("[xx, 1]");
needParens("[#1=xx,3]");
needParens("[#1=xx,#1#]");
needParens("xx.p");
needParens("xx.@p");
needParens("typeof xx;");
needParens("void xx;");
needParens("({ a: xx })");
needParens("({ a: 1, b: xx })");
needParens("({ a: xx, b: 1 })");
needParens("({ a getter: xx })");
needParens("<x a={xx}/>");
doesNotNeedParens("new (xx);");
doesNotNeedParens("new a(xx);");


// Generator expressions cannot be used as LHS, even though they're syntactic 
// sugar for something that looks a lot like an "lvalue return": (f() = 3).

rejectLHS("++ (xx);");
rejectLHS("delete xx;");
rejectLHS("delete (xx);");
rejectLHS("for (xx in []) { }");
rejectLHS("for ((xx) in []) { }");
rejectLHS("try { } catch(xx) { }");
rejectLHS("try { } catch([(xx)]) { }");
rejectLHS("xx += 3;");
rejectLHS("(xx) += 3;");
rejectLHS("xx = 3;");

// Assignment
rejectLHS("        (xx) = 3;");
rejectLHS("var     (xx) = 3;");
rejectLHS("const   (xx) = 3;");
rejectLHS("let     (xx) = 3;");

// Destructuring assignment
rejectLHS("        [(xx)] = 3;");
rejectLHS("var     [(xx)] = 3;");
rejectLHS("const   [(xx)] = 3;");
rejectLHS("let     [(xx)] = 3;");

// Group assignment (Spidermonkey optimization for certain
// destructuring assignments)
rejectLHS("        [(xx)] = [3];");
rejectLHS("var     [(xx)] = [3];");
rejectLHS("const   [(xx)] = [3];");
rejectLHS("let     [(xx)] = [3];");

// Destructuring & group assignment for array comprehensions, just for kicks.
rejectLHS("        [xx] = [3];");
rejectLHS("var     [xx] = [3];");
rejectLHS("const   [xx] = [3];");
rejectLHS("let     [xx] = 3;");
rejectLHS("        [xx] = 3;");
rejectLHS("var     [xx] = 3;");
rejectLHS("const   [xx] = 3;");
rejectLHS("let     [xx] = 3;");

// This is crazy, ambiguous, and/or buggy.
// See https://bugzilla.mozilla.org/show_bug.cgi?id=380237#c23 et seq.
//doesNotNeedParens("(yield xx);");

print("Done!");

function doesNotNeedParens(pat)
{
  print("Testing " + pat);

  var f, ft;
  sanityCheck(pat);

  expect = 'No Error';
  actual = '';
  ft = pat.replace(/xx/, genexp);
    try {
      f = new Function(ft);
      actual = 'No Error';
    } catch(e) {
      print("Unparenthesized genexp SHOULD have been accepted here!");
      actual = e + '';
    }
  reportCompare(expect, actual, summary + ': doesNotNeedParens ' + pat);

  roundTripTest(f);

  // Make sure the decompilation is not over-parenthesized.
  var uf = "" + f;
  if (pat.indexOf("(xx)") != -1)
    overParenTest(f);
  //  else
  //    print("Skipping the over-parenthesization test, because I don't know how to test for over-parenthesization when the pattern doesn't have parens snugly around it.")
}

function needParens(pat)
{
  print("Testing " + pat);

  var f, ft;
  sanityCheck(pat);

  expect = 'SyntaxError';
  actual = '';
  ft = pat.replace(/xx/, genexp);
  try {
    f = new Function(ft);
    print("Unparenthesized genexp should NOT have been accepted here!");
  } catch(e) { 
    /* expected to throw */ 
    actual = e.name;
  }
  reportCompare(expect, actual, summary + ': needParens ' + pat);

  expect = 'No Error';
  actual = '';
  ft = pat.replace(/xx/, genexpParened);
  try {
    f = new Function(ft);
    actual = 'No Error';
  } catch(e) { 
    print("Yikes!"); 
    actual = e + '';
  }
  reportCompare(expect, actual, summary + ': needParens ' + ft);

  roundTripTest(f);
  overParenTest(f);
}

function rejectLHS(pat)
{
  print("Testing " + pat);

  // sanityCheck(pat); // because 'z' should be accepted as an LHS or binding
    
  var ft;
    
  expect = 'SyntaxError';
  actual = '';
  ft = pat.replace(/xx/, genexp)
    try {
      new Function(ft);
      print("That should have been a syntax error!");
      actual = 'No Error';
    } catch(e) { 
      actual = e.name;
    }
  reportCompare(expect, actual, summary + ': rejectLHS');
}


function overParenTest(f)
{
  var uf = "" + f;

  reportCompare(false, uf.indexOf(genexpParened) == -1, summary + 
                ': overParenTest genexp snugly in parentheses: ' + uf);

  if (uf.indexOf(genexpParened) != -1) {
    reportCompare(true, uf.indexOf(genexpParenedTwice) == -1, summary + 
      ': overParensTest decompilation should not be over-parenthesized: ' + uf);
  }
}

function sanityCheck(pat)
{
  expect = '';
  actual = '';

  if (pat.indexOf("xx") == -1)
  {
    actual += "No 'xx' in this pattern? ";
  }

  var f, ft;
  ft = pat.replace(/xx/, "z");
  try {
    f = new Function(ft);
  } catch(e) { 
    actual += "Yowzers! Probably a bogus test!";
  }
  reportCompare(expect, actual, summary + ': sanityCheck ' + pat);
}

function roundTripTest(f)
{
  // Decompile
  var uf = "" + f;

  // Recompile
  expect = 'No Error';
  actual = '';
  var euf;
  try {
    euf = eval("(" + uf + ")");
    actual = 'No Error';
    reportCompare(expect, actual, summary + ': roundTripTest: ' + uf);
  } catch(e) {
    actual = e + '';
    reportCompare(expect, actual, summary + ': roundTripTest: ' + uf);
    return;
  }

  // Decompile again and make sure the decompilations match exactly.
  expect = uf;
  actual = "" + euf;
  reportCompare(expect, actual, summary + ': roundTripTest no round-trip change');
}
