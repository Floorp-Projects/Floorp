/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452498;
var summary = 'TM: upvar2 regression tests';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

// ------- Comment #82 From Gary Kwong [:nth10sd]

// =====

  (function(){function  x(){} {let x = [] }});

// =====

  var f = new Function("new function x(){ return x |= function(){} } ([], function(){})");
  "" + f;

// =====

  uneval(function(){[y] = [x];});

// =====

  function g(code)
  {
    var f = new Function(code);
    f();
  }
  g("for (var x = 0; x < 3; ++x)(new (function(){})());");

// =====
  try
  {
    eval(
      'for(let [\n' +
      'function  x () { M:if([1,,])  }\n'
      );
  }
  catch(ex)
  {
  }

// =====

  try
  {
    function foo(code)
    {
      var c;
      eval("const c, x5 = c;");
    }
    foo();
  }
  catch(ex)
  {
  }

// =====

  var f = new Function("[] = [( '' )()];");
  "" + f;

// =====

  reportCompare(expect, actual, summary);
}
