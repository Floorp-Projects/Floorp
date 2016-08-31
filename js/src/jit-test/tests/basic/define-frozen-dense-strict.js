"use strict";
var t = [4];
var stop;
Object.freeze(t);
do {
    let ok = false;
    stop = inIon();
    try {
        t[1] = 2;
    } catch(e) {
        ok = true;
    }
    assertEq(ok, true);
    assertEq(t.length, 1);
    assertEq(t[1], undefined);
} while (!stop);
