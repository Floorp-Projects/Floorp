/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 321874;
var summary = 'lhs must be a reference in (for lhs in rhs) ...';
var actual = '';
var expect = '';
var section;

printBugNumber(BUGNUMBER);
printStatus (summary);

function a() {}
var b = {foo: 'bar'};

printStatus('for-in tests');

var v;
section = summary + ': for((v) in b);';
expect = 'foo';
printStatus(section);
try
{
  eval('for ((v) in b);');
  actual = v;
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, section);

section = summary + ': function foo(){for((v) in b);};foo();';
expect = 'foo';
printStatus(section);
try
{
  eval('function foo(){ for ((v) in b);}; foo();');
  actual = v;
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, section);

section = summary + ': for(a() in b);';
expect = 'error';
printStatus(section);
try
{
  eval('for (a() in b);');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, section);

section = summary + ': function foo(){for(a() in b);};foo();';
expect = 'error';
printStatus(section);
try
{
  eval('function foo(){ for (a() in b);};foo();');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, section);

section = ': for(new a() in b);';
expect = 'error';
printStatus(section);
try
{
  eval('for (new a() in b);');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

section = ': function foo(){for(new a() in b);};foo();';
expect = 'error';
printStatus(section);
try
{
  eval('function foo(){ for (new a() in b);};foo();');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

section = ': for(void in b);';
expect = 'error';
printStatus(section);
try
{
  eval('for (void in b);');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

section = ': function foo(){for(void in b);};foo();';
expect = 'error';
printStatus(section);
try
{
  eval('function foo(){ for (void in b);};foo();');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

var d = 1;
var e = 2;
expect = 'error';
section = ': for((d*e) in b);';
printStatus(section);
try
{
  eval('for ((d*e) in b);');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

var d = 1;
var e = 2;
expect = 'error';
section = ': function foo(){for((d*e) in b);};foo();';
printStatus(section);
try
{
  eval('function foo(){ for ((d*e) in b);};foo();');
  actual = 'no error';
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

const c = 0;
expect = 0;
section = ': for(c in b);';
printStatus(section);
try
{
  eval('for (c in b);');
  actual = c;
  printStatus('typeof c: ' + (typeof c) + ', c: ' + c);
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);

expect = 0;
section = ': function foo(){for(c in b);};foo();';
printStatus(section);
try
{
  eval('function foo(){ for (c in b);};foo();');
  actual = c;
  printStatus('typeof c: ' + (typeof c) + ', c: ' + c);
}
catch(ex)
{
  printStatus(ex+'');
  actual = 'error';
} 
reportCompare(expect, actual, summary + section);
