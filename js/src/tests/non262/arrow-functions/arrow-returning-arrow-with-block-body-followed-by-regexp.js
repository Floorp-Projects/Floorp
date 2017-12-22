/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function t()
{
  var x = y => z => {} // ASI occurs here
  /Q/;
  return 42;
}

assertEq(t(), 42);

if (typeof reportCompare === "function")
  reportCompare(true, true);
