/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

window.tests.set('propertyTreeSplitting', (function() {
var garbage = [];
var garbageIndex = 0;
return {
    description: "use delete to generate Shape garbage",
    load: (N) => { garbage = new Array(N); },
    unload: () => { garbage = []; garbageIndex = 0; },
    makeGarbage: (N) => {
        function f()
        {
            var a1 = eval;
            delete eval;
            eval = a1;
            var a3 = toString;
            delete toString;
            toString = a3;
        }
        for (var a = 0; a < N; ++a) {
            garbage[garbageIndex++] = new f();
            if (garbageIndex == garbage.length)
                garbageIndex = 0;
        }
    }
};
})());
