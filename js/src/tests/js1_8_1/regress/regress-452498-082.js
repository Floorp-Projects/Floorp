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
  enterFunc ('test');
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
    (function(){new (function ({}, x) { yield (x(1e-81) for (x4 in undefined)) })()})();
  }
  catch(ex)
  {
  }
// =====

  try
  {
    (function(){[(function ([y]) { })() for each (x in [])];})();
  }
  catch(ex)
  {
  }
// =====

  try
  {
    eval('(function(){for(var x2 = [function(id) { return id } for each (x in []) if ([])] in functional) function(){};})();');
  }
  catch(ex)
  {
  }

// =====

  if (typeof window == 'undefined')
    global = this;
  else
    global = window;

  try
  {
    eval('(function(){with(global){1e-81; }for(let [x, x3] = global -= x in []) function(){}})();');
  }
  catch(ex)
  {
  }

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

  var f = new Function("try { with({}) x = x; } catch(\u3056 if (function(){x = x2;})()) { let([] = [1.2e3.valueOf(\"number\")]) ((function(){})()); } ");
  "" + f;

// =====

  var f = new Function("[] = [( '' )()];");
  "" + f;

// =====

  try
  {
    eval(
	  'for(let x;' +
	  '    ([,,,]' +
	  '     .toExponential(new Function(), (function(){}))); [] = {})' +
	  '  for(var [x, x] = * in this.__defineSetter__("", function(){}));'
      );
  }
  catch(ex)
  {
  }

// =====

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
