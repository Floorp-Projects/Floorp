/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 446026;
var summary = 'brian loves eval(s, o)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var b = 45;

// Getting "private" variables
  var obj = (function() {
      var a = 21;
      return {
        // public function must reference 'a'
      fn: function() {a;}
      };
    })();

expect = 'ReferenceError: a is not defined | undefined | 45';
actual = '';

var foo;

try {
  eval('bar = b; foo=a', obj.fn);
} catch (e) {
  actual = e;
}
print(actual += " | " + foo + " | " + bar); // 21
reportCompare(expect, actual, summary);

expect = 'No Error';
actual = 'No Error';

try
{
  eval("", {print:1});
  print(1);
}
catch(ex)
{
  actual = ex + '';
}
reportCompare(expect, actual, summary);
