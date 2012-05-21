/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 313938;
var summary = 'Root access in jsscript.c';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof Script == 'undefined')
{
  print('Test skipped. Script not defined.');
  reportCompare("Script not defined, Test skipped.",
                "Script not defined, Test skipped.",
                summary);
}
else
{
  var str = " 2;".substring(1);
  "1".substring(2);
  expect = Script.prototype.compile(str).toSource();

  var likeString = {
    toString: function() {
      var tmp = str;
      str = null;
      return tmp;
    }
  };

  TWO = 2.0;

  var likeObject = {
    valueOf: function() {
      if (typeof gc == "function")
        gc();
      for (var i = 0; i != 40000; ++i) {
        var tmp = 1e100 * TWO;
      }
      return this;
    }
  }

  var s = Script.prototype.compile(likeString, likeObject);
  var actual = s.toSource();
  printStatus(expect === actual);

  reportCompare(expect, actual, summary);
}
