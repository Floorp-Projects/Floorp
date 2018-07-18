load(libdir + "asserts.js");

var g = newGlobal();
var dbg = Debugger(g);

g.eval(`
function* f() {
    e;
}
`);

// To continue testing after uncaught exception, remember the exception and
// return normal completion.
var currentFrame;
var uncaughtException;
dbg.uncaughtExceptionHook = function(e) {
    uncaughtException = e;
    return {
        return: "uncaught"
    };
};
function testUncaughtException() {
    uncaughtException = undefined;
    var obj = g.eval(`f().next()`);
    assertEq(obj.done, true);
    assertEq(obj.value, 'uncaught');
    assertEq(uncaughtException instanceof TypeError, true);
}

// Just continue
dbg.onExceptionUnwind = function(frame) {
    return undefined;
};
assertThrowsInstanceOf(() => g.eval(`f().next();`), g.ReferenceError);

// Forced early return
dbg.onExceptionUnwind = function(frame) {
    currentFrame = frame;
    return {
        return: "foo"
    };
};
var obj = g.eval(`f().next()`);
assertEq(obj.done, true);
assertEq(obj.value, "foo");

// Bad resumption value
dbg.onExceptionUnwind = function(frame) {
    currentFrame = frame;
    return {declaim: "gadzooks"};
};
testUncaughtException();
