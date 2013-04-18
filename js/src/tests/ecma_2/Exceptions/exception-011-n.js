/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var SECTION = "exception-011";
var VERSION = "ECMA_2";
startTest();
var TITLE   = "Don't Crash throwing undefined";

writeHeaderToLog( SECTION + " "+ TITLE);

print("Undefined throw test.");

DESCRIPTION = "throw undefined";
EXPECTED = "error";

new TestCase( SECTION,  "throw undefined",  "error", eval("throw (void 0)") );

test();

print("FAILED!: Should have exited with uncaught exception.");



