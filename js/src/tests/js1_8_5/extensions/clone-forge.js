// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function assertThrows(f) {
    var ok = false;
    try {
        f();
    } catch (exc) {
        ok = true;
    }
    if (!ok)
        throw new TypeError("Assertion failed: " + f + " did not throw as expected");
}

// Don't allow forging bogus Date objects.
var buf = serialize(new Date(NaN));
var a = [1/0, -1/0,
         Number.MIN_VALUE, -Number.MIN_VALUE,
         Math.PI, 1286523948674.5,
         Number.MAX_VALUE, -Number.MAX_VALUE,
         8.64e15 + 1, -(8.64e15 + 1)];
for (var i = 0; i < a.length; i++) {
    var n = a[i];
    var nbuf = serialize(n);
    for (var j = 0; j < 8; j++)
        buf[j + 8] = nbuf[j];
    assertThrows(function () { deserialize(buf); });
}

reportCompare(0, 0);
