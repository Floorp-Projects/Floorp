/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*
 * If the tracer fails to notice that computeThis() will produce a wrapped
 * primitive, then we may get:
 *
 * Assertion failure: thisObj == globalObj
 */

var HOTLOOP = this.tracemonkey ? tracemonkey.HOTLOOP : 8;
var a;
function f(n) {
    for (var i = 0; i < HOTLOOP; i++)
        if (i == HOTLOOP - 2)
            a = this;
}

/*
 * Various sorts of events can cause the global to be reshaped, which
 * resets our loop counts. Furthermore, we don't record a branch off a
 * trace until it has been taken HOTEXIT times. So simply calling the
 * function twice may not be enough to ensure that the 'a = this' branch
 * gets recorded. This is probably excessive, but it'll work.
 */
f.call("s", 1);
f.call("s", 1);
f.call("s", 1);
f.call("s", 1);
f.call("s", 1);
f.call("s", 1);
f.call("s", 1);
f.call("s", 1);
assertEq(typeof a, "object");
assertEq("" + a, "s");
