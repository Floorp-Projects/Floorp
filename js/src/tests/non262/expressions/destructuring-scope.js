/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 'none';
var summary = 'Test destructuring assignments for differing scopes';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

function f() {
  var x = 3;
  if (x > 0) {
    let {a:x} = {a:7};
    if (x != 7) 
      throw "fail";
  }
  if (x != 3) 
    throw "fail";
}

function g() {
  // Before JS1.7's destructuring for…in was fixed to match JS1.8's,
  // the expected values were a == "x" and b == 7.
  for (var [a,b] in {x:7}) {
    if (a !== "x" || typeof b !== "undefined")
      throw "fail";
  }

  {
    // Before JS1.7's destructuring for…in was fixed to match JS1.8's,
    // the expected values were a == "y" and b == 8.
    for (let [a,b] in {y:8}) {
      if (a !== "y" || typeof b !== "undefined")
        throw "fail";
    }
  }

  if (a !== "x" || typeof b !== "undefined")
    throw "fail";
}

f();
g();

if (typeof a != "undefined" || typeof b != "undefined" || typeof x != "undefined")
  throw "fail";

function h() {
  // Before JS1.7's destructuring for…in was fixed to match JS1.8's,
  // the expected values were a == "x" and b == 9.
  for ([a,b] in {z:9}) {
    if (a !== "z" || typeof b !== "undefined")
      throw "fail";
  }
}

h();

if (a !== "z" || typeof b !== "undefined")
  throw "fail";

reportCompare(expect, actual, summary);
