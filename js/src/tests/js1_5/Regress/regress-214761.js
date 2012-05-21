/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 214761;
var summary = 'Crash Regression from bug 208030';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

options('strict');

var code = "var bar1=new Array();\n" +
  "bar1[0]='foo';\n" +
  "var bar2=document.all&&navigator.userAgent.indexOf('Opera')==-1;\n" +
  "var bar3=document.getElementById&&!document.all;\n" +
  "var bar4=document.layers;\n" +
  "function foo1(){\n" +
  "return false;}\n" +
  "function foo2(){\n" +
  "return false;}\n" +
  "function foo3(){\n" +
  "return false;}\n" +
  "function foo4(){\n" +
  "return false;}\n" +
  "function foo5(){\n" +
  "return false;}\n" +
  "function foo6(){\n" +
  "return false;}\n" +
  "function foo7(){\n" +
  "return false;}";

try
{
  eval(code);
}
catch(e)
{
}

reportCompare(expect, actual, summary);
