// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     Function_object.js
   Description:  'Testing Function objects'

   Author:       Nick Lerissa
   Date:         April 17, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
startTest();
var TITLE = 'functions: Function_object';

writeHeaderToLog('Executing script: Function_object.js');
writeHeaderToLog( SECTION + " "+ TITLE);


function a_test_function(a,b,c)
{
  return a + b + c;
}

f = a_test_function;


new TestCase( SECTION, "f.name",
	      'a_test_function',  f.name);

new TestCase( SECTION, "f.length",
	      3,  f.length);

new TestCase( SECTION, "f.arity",
	      3,  f.arity);

new TestCase( SECTION, "f(2,3,4)",
	      9,  f(2,3,4));

var fnName = (version() == 120) ? '' : 'anonymous';

new TestCase( SECTION, "(new Function()).name",
	      fnName, (new Function()).name);

new TestCase( SECTION, "(new Function()).toString()",
	      'function ' + fnName + '() {\n}',  (new Function()).toString());

test();

