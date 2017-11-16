/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function errors() {
    return [
        // The following two errors MUST NOT be captured.
        new Error("This is an error: " + Math.random()),
        new RangeError("This is a RangeError: " + Math.random()),
        "This is a string: " + Math.random(),
        null,
        undefined,
        Math.random(),
        {},

        // The following errors MUST be captured.
        new TypeError("This is a TypeError: " + Math.random()),
        new SyntaxError("This is a SyntaxError: " + Math.random()),
        new ReferenceError("This is a ReferenceError: " + Math.random())
    ]
}

function isDeveloperError(e) {
    if (e == null || typeof e != "object") {
        return false;
    }

    return e.constructor == TypeError
        || e.constructor == SyntaxError
        || e.constructor == ReferenceError;
}

function run_test() {
    ChromeUtils.clearRecentJSDevError();
    Assert.equal(ChromeUtils.recentJSDevError, undefined);

    for (let exn of errors()) {
        ChromeUtils.clearRecentJSDevError();
        try {
            throw exn;
        } catch (e) {
            // Discard error.
        }
        if (isDeveloperError(exn)) {
            Assert.equal(ChromeUtils.recentJSDevError.message, "" + exn);
        } else {
            Assert.equal(ChromeUtils.recentJSDevError, undefined);
        }
        ChromeUtils.clearRecentJSDevError();
        Assert.equal(ChromeUtils.recentJSDevError, undefined);
    }
};

