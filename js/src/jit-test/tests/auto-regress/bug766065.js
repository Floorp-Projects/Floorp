// |jit-test| error:ReferenceError

// Binary: cache/js-dbg-32-19bfe36cace8-linux
// Flags: -m -n -a
//

function testApplyCallHelper(f) {
    for (var i = 0; i < 10; ++i) dumpStack.apply(this,[0,1]);
}
function testApplyCall() {
    var r = testApplyCallHelper(function (a0,a1,a2,a3,a4,a5,a6,a7) { x = [a0,a1,a2,a3,a4,a5,a6,a7]; });
}
testApplyCall();
