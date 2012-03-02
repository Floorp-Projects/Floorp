// onPop surfaces.
load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
var dbg = new Debugger(g);

// Assigning a bogus value to Debugger.Frame.prototype.onPop raises a TypeError.
function test(badValue) {
    print("store " + uneval(badValue) + " in Debugger.Frame.prototype.onPop");

    var log;
    dbg.onDebuggerStatement = function handleDebugger(frame) {
        log += "d";
        assertThrowsInstanceOf(function () { frame.onPop = badValue; }, TypeError);
    };

    log = "";
    g.eval("debugger");
    assertEq(log, "d");
}

test(null);
test(false);
test(1);
test("stringy");
test({});
test([]);

// Getting and setting the prototype's onPop is an error.
assertThrowsInstanceOf(function () { Debugger.Frame.prototype.onPop; }, TypeError);
assertThrowsInstanceOf(
    function () { Debugger.Frame.prototype.onPop = function () {}; },
    TypeError);

// The getters and setters correctly check the type of their 'this' argument.
var descriptor = Object.getOwnPropertyDescriptor(Debugger.Frame.prototype, 'onPop');
assertEq(descriptor.configurable, true);
assertEq(descriptor.enumerable, false);
assertThrowsInstanceOf(function () { descriptor.get.call({}); }, TypeError);
assertThrowsInstanceOf(function () { descriptor.set.call({}, function() {}); }, TypeError);
