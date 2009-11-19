/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Blake Kaplan
 */

var gTestfile = 'regress-520572.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 520572;
var summary = 'watch should innerize the object being watched';
var actual = 0;
var expect = 2;


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  if ("evalcx" in this) {
      // shell
      let s = evalcx("split");
      s.n = 0;
      evalcx('this.watch("x", function(){ n++; }); this.x = 4; x = 6', s);
      actual = s.n;
      reportCompare(expect, actual, summary);
  } else {
      // browser
      this.watch('x', function(){ actual++; });
      this.x = 4;
      x = 6;
      reportCompare(expect, actual, summary);
  }

  exitFunc ('test');
}
