/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


DESCRIPTION = "Non-character escapes in identifiers negative test.";
EXPECTED = "error";

enterFunc ("test");

printStatus ("Non-character escapes in identifiers negative test.");
printBugNumber (23607);

eval("\u0020 = 5");
reportCompare('PASS', 'FAIL', "Previous statement should have thrown an error.");

exitFunc ("test");

