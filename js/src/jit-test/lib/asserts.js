/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


load(libdir + "non262.js");

if (typeof assertWarning === 'undefined') {
    var assertWarning = function assertWarning(f, pattern) {
        enableLastWarning();

        // Verify that a warning is issued.
        clearLastWarning();
        f();
        var warning = getLastWarning();
        clearLastWarning();

        disableLastWarning();

        if (warning) {
            if (!warning.message.match(pattern)) {
                throw new Error(`assertWarning failed: "${warning.message}" does not match "${pattern}"`);
            }
            return;
        }

        throw new Error("assertWarning failed: no warning");
    };
}

if (typeof assertNoWarning === 'undefined') {
    var assertNoWarning = function assertNoWarning(f, msg) {
        enableLastWarning();

        // Verify that no warning is issued.
        clearLastWarning();
        f();
        var warning = getLastWarning();
        clearLastWarning();

        disableLastWarning();

        if (warning) {
            if (msg) {
                print("assertNoWarning: " + msg);
            }

            throw Error("assertNoWarning: Unexpected warning when calling: " + f);
        }
    };
}

if (typeof assertErrorMessage === 'undefined') {
    var assertErrorMessage = function assertErrorMessage(f, ctor, test) {
        try {
            f();
        } catch (e) {
            // Propagate non-specific OOM errors, we never test for these with
            // assertErrorMessage, as there is no meaningful ctor.
            if (e === "out of memory")
                throw e;
            if (!(e instanceof ctor))
                throw new Error("Assertion failed: expected exception " + ctor.name + ", got " + e);
            if (typeof test == "string") {
                if (test != e.message)
                    throw new Error("Assertion failed: expected " + test + ", got " + e.message);
            } else {
                if (!test.test(e.message))
                    throw new Error("Assertion failed: expected " + test.toString() + ", got " + e.message);
            }
            return;
        }
        throw new Error("Assertion failed: expected exception " + ctor.name + ", no exception thrown");
    };
}

if (typeof assertTypeErrorMessage === 'undefined') {
    var assertTypeErrorMessage = function assertTypeErrorMessage(f, test) {
      assertErrorMessage(f, TypeError, test);
    };
}

if (typeof assertRangeErrorMessage === 'undefined') {
    var assertRangeErrorMessage = function assertRangeErrorMessage(f, test) {
      assertErrorMessage(f, RangeError, test);
    };
}
