/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 306738;
var summary = 'uneval() on objects with getter or setter';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

actual = uneval(
  {
    get foo()
    {
      return "foo";
    }
  });

expect = '({get foo()\n\
    {\n\
      return "foo";\n\
    }})';
 
compareSource(expect, actual, summary);
