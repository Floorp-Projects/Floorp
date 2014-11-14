// |jit-test| error:Error

// Binary: cache/js-dbg-32-1301a72b1c39-linux
// Flags: --ion-eager
//

load(libdir + "evalInFrame.js");

[1,2,3,4,(':'),6,7,8].forEach(
    function(x) {
        assertEq(evalInFrame(0, ('^')), x);
    }
);
