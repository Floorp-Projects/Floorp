/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 343765;
var summary = 'Function defined in a let statement/expression does not work correctly outside the let scope';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  print("SECTION 1");
  try {
    let (a = 2, b = 3) { function f() { return String([a,b]); } f(); throw 42; }
  } catch (e) {
    f();
  }

  reportCompare(expect, actual, summary);

  print("SECTION 2");
  try {
    with ({a:2,b:3}) { function f() { return String([a,b]); } f(); throw 42; }
  } catch (e) {
    f();
  }

  print("SECTION 3");
  function g3() {
    print("Here!");
    with ({a:2,b:3}) {
      function f() {
        return String([a,b]);
      }

      f();
      return f;
    }
  }

  k = g3();
  k();

  print("SECTION 4");
  function g4() {
    print("Here!");
    let (a=2,b=3) {
      function f() {
        return String([a,b]);
      }

      f();
      return f;
    }
  }

  k = g4();
  k();

  print("SECTION 5");
  function g5() {
    print("Here!");
    let (a=2,b=3) {
      function f() {
        return String([a,b]);
      }

      f();
      yield f;
    }
  }

  k = g5().next();
  k();

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

function returnResult(a)
{
  return a + '';
}
