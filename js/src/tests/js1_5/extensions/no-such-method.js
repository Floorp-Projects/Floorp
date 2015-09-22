// |reftest| skip-if(!xulRuntime.shell)
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 196097;
var summary = '__noSuchMethod__ handler';
var actual = '';
var expect = '';

enableNoSuchMethod();

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var o = {
  __noSuchMethod__: function (id, args)
  {
    return(id + '('+args.join(',')+')');
  }
};

status = summary + ' ' + inSection(1) + ' ';
actual = o.foo(1,2,3);
expect = 'foo(1,2,3)';
reportCompare(expect, actual, status);

status = summary + ' ' + inSection(2) + ' ';
actual = o.bar(4,5);
expect = 'bar(4,5)';
reportCompare(expect, actual, status);

status = summary + ' ' + inSection(3) + ' ';
actual = o.baz();
expect = 'baz()';
reportCompare(expect, actual, status);
