// |reftest| skip-if(!xulRuntime.shell)
/* -*- Mode: js2; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/
 */

// Deserialize a serialization buffer containing a reference to a
// SharedArrayBuffer buffer object enough times and we will crash because of a
// reference counting bug.

if (!this.SharedArrayBuffer) {
    reportCompare(true,true);
    quit(0);
}

let x = new SharedArrayBuffer(1);
let y = serialize(x);
x = null;

// If the bug is present this loop usually crashes quickly during
// deserialization because the memory has become unmapped.

for (let i=0 ; i < 50 ; i++ ) {
    let obj = deserialize(y);
    let z = new Int8Array(obj);
    z[0] = 0;
}

reportCompare(true, true);

