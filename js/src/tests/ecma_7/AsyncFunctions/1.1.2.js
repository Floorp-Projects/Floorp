/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

async function test() { }

var anon = async function() { }

assertEq(test.name, "test");
assertEq(anon.name, "");

if (typeof reportCompare === "function")
    reportCompare(true, true);
