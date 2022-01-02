/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function assertEqArray(actual, expected) {
    if (actual.length != expected.length) {
        throw new Error(
            "array lengths not equal: got " +
            JSON.stringify(actual) + ", expected " + JSON.stringify(expected));
    }

    for (var i = 0; i < actual.length; ++i) {
        if (actual[i] != expected[i]) {
        throw new Error(
            "arrays not equal at element " + i + ": got " +
            JSON.stringify(actual) + ", expected " + JSON.stringify(expected));
        }
    }
}


