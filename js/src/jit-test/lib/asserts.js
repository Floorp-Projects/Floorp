/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


load(libdir + "../../tests/ecma_6/shell.js");

if (typeof assertWarning === 'undefined') {
    var assertWarning = function assertWarning(f, errorClass, msg) {
        var hadWerror = options().split(",").indexOf("werror") !== -1;

        // Ensure the "werror" option is disabled.
        if (hadWerror)
            options("werror");

        try {
            f();
        } catch (exc) {
            if (hadWerror)
                options("werror");

            // print() rather than throw a different exception value, in case
            // the caller wants exc.stack.
            if (msg)
                print("assertWarning: " + msg);
            print("assertWarning: Unexpected exception calling " + f +
                  " with warnings-as-errors disabled");
            throw exc;
        }

        // Enable the "werror" option.
        options("werror");

        try {
            assertThrowsInstanceOf(f, errorClass, msg);
        } catch (exc) {
            if (msg)
                print("assertWarning: " + msg);
            throw exc;
        } finally {
            if (!hadWerror)
                options("werror");
        }
    };
}

if (typeof assertNoWarning === 'undefined') {
    var assertNoWarning = function assertWarning(f, msg) {
        // Ensure the "werror" option is enabled.
        var hadWerror = options().split(",").indexOf("werror") !== -1;
        if (!hadWerror)
            options("werror");

        try {
            f();
        } catch (exc) {
            if (msg)
                print("assertNoWarning: " + msg);
            print("assertNoWarning: Unexpected exception calling " + f +
                  "with warnings-as-errors enabled");
            throw exc;
        } finally {
            if (!hadWerror)
                options("werror");
        }
    };
}

if (typeof assertErrorMessage === 'undefined') {
    var assertErrorMessage = function assertErrorMessage(f, ctor, test) {
        try {
            f();
        } catch (e) {
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
