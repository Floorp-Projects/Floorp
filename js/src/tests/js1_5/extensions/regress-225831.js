/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    15 Nov 2003
 * SUMMARY: Stressing the byte code generator
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=225831
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 225831;
var summary = 'Stressing the byte code generator';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


function f() { return {x: 0}; }

var N = 300;
var a = new Array(N + 1);
a[N] = 10;
a[0] = 100;


status = inSection(1);

// build string of the form ++(a[++f().x + ++f().x + ... + ++f().x]) which
// gives ++a[N]
var str = "".concat("++(a[", repeat_str("++f().x + ", (N - 1)), "++f().x])");

// Use Script constructor instead of simple eval to test Rhino optimizer mode
// because in Rhino, eval always uses interpreted mode.
if (typeof Script == 'undefined')
{
  print('Test skipped. Script not defined.');
}
else
{
  var script = new Script(str);
  script();

  actual = a[N];
  expect = 11;
}
addThis();

status = inSection(2);


// build string of the form (a[f().x-- + f().x-- + ... + f().x--])--
// which should give (a[0])--
if (typeof Script == 'undefined')
{
  print('Test skipped. Script not defined.');
}
else
{
  str = "".concat("(a[", repeat_str("f().x-- + ", (N - 1)), "f().x--])--");
  script = new Script(str);
  script();

  actual = a[0];
  expect = 99;
}
addThis();


status = inSection(3);

// build string of the form [[1], [1], ..., [1]]
if (typeof Script == 'undefined')
{
  print('Test skipped. Script not defined.');
}
else
{
  str = "".concat("[", repeat_str("[1], ", (N - 1)), "[1]]");
  script = new Script(str);
  script();

  actual = uneval(script());
  expect = str;
}
addThis();


status = inSection(4);

// build string of the form ({1:{a:1}, 2:{a:1}, ... N:{a:1}})
if (typeof Script == 'undefined')
{
  print('Test skipped. Script not defined.');
}
else
{
  str = function() {
    var arr = new Array(N+1);
    arr[0] = "({";
    for (var i = 1; i < N; ++i) {
      arr[i] = i+":{a:1}, ";
    }
    arr[N] = N+":{a:1}})";
    return "".concat.apply("", arr);
  }();

  script = new Script(str);
  script();

  actual = uneval(script());
  expect = str;
}
addThis();




//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function repeat_str(str, repeat_count)
{
  var arr = new Array(--repeat_count);
  while (repeat_count != 0)
    arr[--repeat_count] = str;
  return str.concat.apply(str, arr);
}


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
