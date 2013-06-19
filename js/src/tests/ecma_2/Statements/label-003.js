/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The colon for a labeled statement may be on a separate line.
var x;
label
: {
    x = 1;
    break label;
    x = 2;
}
assertEq(x, 1);
reportCompare(0, 0, 'ok');
