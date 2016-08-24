// |jit-test| --no-threads; --ion-eager; --ion-shared-stubs=off
setJitCompilerOption('ion.forceinlineCaches', 1);
function foo(t) {
    "use strict";
    var stop;
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
}
var t = [4];
Object.freeze(t);
foo(t);
foo(t);
