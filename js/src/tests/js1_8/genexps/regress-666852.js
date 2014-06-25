/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 665286;
var summary = 'yield in comprehension RHS';
var actual = '';
var expect = '';

function reported() {
    [1 for (x in yield)]
}

reportCompare(reported.isGenerator(), true, "reported case: is generator");
reportCompare(typeof reported(), "object", "reported case: calling doesn't crash");
