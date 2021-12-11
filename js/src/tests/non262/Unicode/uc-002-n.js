/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var BUGNUMBER = 23607;
DESCRIPTION = "Non-character escapes in identifiers negative test.";

printStatus ("Non-character escapes in identifiers negative test.");
printBugNumber (BUGNUMBER);

eval("\u0020 = 5");
reportCompare('PASS', 'FAIL', "Previous statement should have thrown an error.");

