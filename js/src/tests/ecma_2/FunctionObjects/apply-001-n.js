/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


print("STATUS: f.apply crash test.");

print("BUGNUMBER: 21836");

function f ()
{
}

var SECTION = "apply-001-n";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "f.apply(2,2) doesn't crash";

writeHeaderToLog( SECTION + " "+ TITLE);

DESCRIPTION = "f.apply(2,2) doesn't crash";
EXPECTED = "error";

new TestCase( SECTION,  "f.apply(2,2) doesn't crash",     "error",    eval("f.apply(2,2)") );

test();


