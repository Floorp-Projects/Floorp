/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 422269;
var summary = 'Compile-time let block should not capture runtime references';
var actual = 'referenced only by stack and closure';
var expect = 'referenced only by stack and closure';


//-----------------------------------------------------------------------------
test();

//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  function f()
  {
    let m = {sin: Math.sin};
    (function holder() { m.sin(1); })();
    return m;
  }

  if (typeof findReferences == 'undefined')
  {
    expect = actual = 'Test skipped';
    print('Test skipped. Requires findReferences function.');
  }
  else
  {
    var x = f();
    var refs = findReferences(x);

    // At this point, x should only be referenced from the stack --- the
    // variable 'x' itself, and any random things the conservative scanner
    // finds --- and possibly from the 'holder' closure, which could itself
    // be held alive for random reasons. Remove those from the refs list, and 
    // then complain if anything is left.
    for (var edge in refs) {
      // Remove references from roots, like the stack.
      if (refs[edge].every(function (r) r === null))
        delete refs[edge];
      // Remove references from the closure, which could be held alive for
      // random reasons.
      else if (refs[edge].length === 1 &&
               typeof refs[edge][0] === "function" &&
               refs[edge][0].name === "holder")
        delete refs[edge];
    }

    if (Object.keys(refs).length != 0)
        actual = "unexpected references to the result of f: " + Object.keys(refs).join(", ");
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
