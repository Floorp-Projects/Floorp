"use strict";
var t = [4];
var stop;
Object.freeze(t);
do {
    let ok = false;
    stop = inIon();
    try {
        t[0] = 2;
    } catch(e) {
        ok = true;
    }
    assertEq(ok, true);
} while (!stop);
