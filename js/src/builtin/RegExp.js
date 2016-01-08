/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ES6 draft rev34 (2015/02/20) 21.2.5.3 get RegExp.prototype.flags
function RegExpFlagsGetter() {
    // Steps 1-2.
    var R = this;
    if (!IsObject(R))
        ThrowTypeError(JSMSG_NOT_NONNULL_OBJECT, R === null ? "null" : typeof R);

    // Step 3.
    var result = "";

    // Steps 4-6.
    if (R.global)
        result += "g";

    // Steps 7-9.
    if (R.ignoreCase)
        result += "i";

    // Steps 10-12.
    if (R.multiline)
        result += "m";

    // Steps 13-15.
    if (R.unicode)
         result += "u";

    // Steps 16-18.
    if (R.sticky)
        result += "y";

    // Step 19.
    return result;
}
_SetCanonicalName(RegExpFlagsGetter, "get flags");

// ES6 draft rc1 21.2.5.14.
function RegExpToString()
{
    // Steps 1-2.
    var R = this;
    if (!IsObject(R))
        ThrowTypeError(JSMSG_NOT_NONNULL_OBJECT, R === null ? "null" : typeof R);

    // Steps 3-4.
    var pattern = R.source;

    // Steps 5-6.
    var flags = R.flags;

    // Step 7.
    return '/' + pattern + '/' + flags;
}

// ES6 21.2.5.2.
// NOTE: This is not RegExpExec (21.2.5.2.1).
function RegExp_prototype_Exec(string) {
    // Steps 1-3.
    var R = this;
    if (!IsObject(R) || !IsRegExpObject(R))
        return callFunction(CallRegExpMethodIfWrapped, R, string, "RegExp_prototype_Exec");

    // Steps 4-5.
    var S = ToString(string);

    // Step 6.
    return RegExpBuiltinExec(R, S, false);
}

// ES6 21.2.5.2.1.
function RegExpExec(R, S, forTest) {
    // Steps 1-2 (skipped).

    // Steps 3-4.
    var exec = R.exec;

    // Step 5.
    // If exec is the original RegExp.prototype.exec, use the same, faster,
    // path as for the case where exec isn't callable.
    if (exec === RegExp_prototype_Exec || !IsCallable(exec)) {
        // ES6 21.2.5.2 steps 1-2, 4-5 (skipped) for optimized case.

        // Steps 6-7 or ES6 21.2.5.2 steps 3, 6 for optimized case.
        return RegExpBuiltinExec(R, S, forTest);
    }

    // Steps 5.a-b.
    var result = callContentFunction(exec, R, S);

    // Step 5.c.
    if (typeof result !== "object")
        ThrowTypeError(JSMSG_EXEC_NOT_OBJORNULL);

    // Step 5.d.
    return forTest ? result !== null : result;
}

// ES6 21.2.5.2.2.
function RegExpBuiltinExec(R, S, forTest) {
    // ES6 21.2.5.2.1 step 6.
    // This check is here for RegExpTest.  RegExp_prototype_Exec does same
    // thing already.
    if (!IsRegExpObject(R))
        return callFunction(CallRegExpMethodIfWrapped, R, R, S, forTest, "RegExpBuiltinExec");

    // Step 1-2 (skipped).

    // Steps 4-5.
    var lastIndex = ToLength(R.lastIndex);

    // Steps 6-7.
    var global = !!R.global;

    // Steps 8-9.
    var sticky = !!R.sticky;

    // Step 10.
    if (!global && !sticky) {
        lastIndex = 0;
    } else {
        if (lastIndex < 0 || lastIndex > S.length) {
            // Steps 15.a.i-ii, 15.c.i.1-2.
            R.lastIndex = 0;
            return forTest ? false : null;
        }
    }

    if (forTest) {
        // Steps 3, 11-17, except 15.a.i-ii, 15.c.i.1-2.
        var endIndex = RegExpTester(R, S, lastIndex, sticky);
        if (endIndex == -1) {
            // Steps 15.a.i-ii, 15.c.i.1-2.
            R.lastIndex = 0;
            return false;
        }

        // Step 18.
        if (global || sticky)
            R.lastIndex = endIndex;

        return true;
    }

    // Steps 3, 11-17, except 15.a.i-ii, 15.c.i.1-2.
    var result = RegExpMatcher(R, S, lastIndex, sticky);
    if (result === null) {
        // Steps 15.a.i-ii, 15.c.i.1-2.
        R.lastIndex = 0;
    } else {
        // Step 18.
        if (global || sticky)
            R.lastIndex = result.index + result[0].length;
    }

    return result;
}

// ES6 21.2.5.13.
function RegExpTest(string) {
    // Steps 1-2.
    var R = this;
    if (!IsObject(R))
        ThrowTypeError(JSMSG_NOT_NONNULL_OBJECT, R === null ? "null" : typeof R);

    // Steps 3-4.
    var S = ToString(string);

    // Steps 5-6.
    return RegExpExec(R, S, true);
}
_SetCanonicalName(RegExpToString, "toString");
