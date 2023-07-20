/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const BUGNUMBER = 1707974;
const SUMMARY = "Test mismatched placement error";

printBugNumber(BUGNUMBER);
printStatus(SUMMARY);

let actual = "";
const expect = "SyntaxError: getter and setter for private name #x \
should either be both static or non-static";

try
{
  eval(`
    class A {
      static set #x(_) {}
      get #x() {}
    }
  `);
}
catch(ex)
{
  actual = ex + "";
}

reportCompare(expect, actual, SUMMARY);
