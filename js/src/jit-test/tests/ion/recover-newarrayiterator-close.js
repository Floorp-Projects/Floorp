// |jit-test| --no-threads
"use strict";

function test() {
    for (var k = 0; k < 35; ++k) {
        try {
            (function (x) {
                (x ? 0 : ([y] = []));
            })();
        } catch (e) {}
    }
}

for (var i = 0; i < 35; i++)
    test();
