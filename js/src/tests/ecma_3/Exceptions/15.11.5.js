/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    4 Oct 2010
 * SUMMARY: Error instances have no special properties beyond those inherited
 *          from the Error prototype object
 */
//-----------------------------------------------------------------------------
var summary = 'Error instances have no special properties beyond those inherited the Error prototype object';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printStatus (summary);

  var actual = { name: "no error", message: "no message" };
  try {
      new Error.prototype;
  } catch (e) {
      actual = e;
  }

  reportCompare("TypeError", actual.name, "must be a TypeError");
  reportCompare(true, /not a constructor/.test(actual.message),
                "message must indicate not a constructor");
}
