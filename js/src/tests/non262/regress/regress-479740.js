/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 479740;
var summary = 'TM: Do not crash @ TraceRecorder::test_property_cache';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);


try
{
  eval(
    '  function f() {' +
    '    for ( foobar in (function() {' +
    '          for (var x = 0; x < 2; ++x) {' +
    '            if (x % 2 == 1) { yield (this.z.z) = function(){} }' +
    '            eval("this", false);' +
    '          }' +
    '        })());' +
    '      function(){}' +
    '  }' +
    '  f();'
    );
}
catch(ex)
{
}


reportCompare(expect, actual, summary);
