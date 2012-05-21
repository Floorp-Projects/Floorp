/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 343675;
var summary = 'Allow keywords, reserved words as function names';
var actual = '';
var expect = 'No Error';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var words = [
    'break', 'else', 'new', 'var', 'case', 'finally', 'return', 'void',
    'catch', 'for', 'switch', 'while', 'continue', 'function', 'this',
    'with', 'default', 'if', 'throw', 'delete', 'in', 'try', 'do',
    'instanceof', 'typeof',
    'abstract', 'enum', 'int', 'short', 'boolean', 'export', 'interface',
    'static', 'byte', 'extends', 'long', 'super', 'char', 'final', 'native',
    'synchronized', 'class', 'float', 'package', 'throws', 'const', 'goto',
    'private', 'transient', 'debugger', 'implements', 'protected', 'volatile',
    'double', 'import', 'public'];

  for (var i = 0; i < words.length; i++)
  {
    try
    {
      actual = 'No Error';
      eval('function ' + words[i] + '() {}');
    }
    catch(ex)
    {
      actual = ex + '';
    }
    reportCompare(expect, actual, summary + ': ' + words[i]);
  }

  exitFunc ('test');
}
