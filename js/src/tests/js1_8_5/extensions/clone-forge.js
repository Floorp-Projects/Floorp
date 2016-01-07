// |reftest| skip-if(!xulRuntime.shell)
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

function byteArray(str) {
    return str.split('').map(c => c.charCodeAt(0));
}

// Don't allow forging bogus Date objects.
var mutated = byteArray(serialize(new Date(NaN)).clonebuffer);

var a = [1/0, -1/0,
         Number.MIN_VALUE, -Number.MIN_VALUE,
         Math.PI, 1286523948674.5,
         Number.MAX_VALUE, -Number.MAX_VALUE,
         8.64e15 + 1, -(8.64e15 + 1)];
for (var i = 0; i < a.length; i++) {
    var n = a[i];
    var nbuf = serialize(n);
    var data = byteArray(nbuf.clonebuffer);
    for (var j = 0; j < 8; j++)
      mutated[j+8] = data[j];
    nbuf.clonebuffer = String.fromCharCode.apply(null, mutated);
    assertThrows(function () { deserialize(nbuf); });
}

reportCompare(0, 0);
