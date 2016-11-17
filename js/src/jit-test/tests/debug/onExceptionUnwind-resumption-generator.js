load(libdir + "asserts.js");

var g = newGlobal();
var dbg = Debugger(g);

g.eval(`
function* f() {
    e;
}
`);

// To continue testing after uncaught exception, remember the exception and
// return normal completeion.
var currentFrame;
var uncaughtException;
dbg.uncaughtExceptionHook = function(e) {
    uncaughtException = e;
    return {
        return: currentFrame.eval("({ done: true, value: 'uncaught' })").return
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

// Should return object.
dbg.onExceptionUnwind = function(frame) {
    currentFrame = frame;
    return {
        return: "foo"
    };
};
testUncaughtException();

// The object should have `done` property and `value` property.
dbg.onExceptionUnwind = function(frame) {
    currentFrame = frame;
    return {
        return: frame.eval("({})").return
    };
};
testUncaughtException();

// The object should have `done` property.
dbg.onExceptionUnwind = function(frame) {
    currentFrame = frame;
    return {
        return: frame.eval("({ value: 10 })").return
    };
};
testUncaughtException();

// The object should have `value` property.
dbg.onExceptionUnwind = function(frame) {
    currentFrame = frame;
    return {
        return: frame.eval("({ done: true })").return
    };
};
testUncaughtException();

// `done` property should be a boolean value.
dbg.onExceptionUnwind = function(frame) {
    currentFrame = frame;
    return {
        return: frame.eval("({ done: 10, value: 10 })").return
    };
};
testUncaughtException();

// `done` property shouldn't be an accessor.
dbg.onExceptionUnwind = function(frame) {
    currentFrame = frame;
    return {
        return: frame.eval("({ get done() { return true; }, value: 10 })").return
    };
};
testUncaughtException();

// `value` property shouldn't be an accessor.
dbg.onExceptionUnwind = function(frame) {
    currentFrame = frame;
    return {
        return: frame.eval("({ done: true, get value() { return 10; } })").return
    };
};
testUncaughtException();

// The object shouldn't be a Proxy.
dbg.onExceptionUnwind = function(frame) {
    currentFrame = frame;
    return {
        return: frame.eval("new Proxy({ done: true, value: 10 }, {})").return
    };
};
testUncaughtException();

// Correct resumption value.
dbg.onExceptionUnwind = function(frame) {
    currentFrame = frame;
    return {
        return: frame.eval("({ done: true, value: 10 })").return
    };
};
var obj = g.eval(`f().next()`);
assertEq(obj.done, true);
assertEq(obj.value, 10);
