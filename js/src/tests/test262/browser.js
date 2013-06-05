/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test262 function $INCLUDE loads a file with support functions for the tests.
 * Our initial import strategy will be to attempt to load all these files
 * unconditionally (this may change at some point), so just ignore the call.
 * This function replaces one in shell.js.
 */
function $INCLUDE(file) {}
