// Invalid offsets result in exceptions, not bogus results.

load(libdir + "asserts.js");

var g = newGlobal();
var dbg = Debugger(g);
var hits;
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.script.getOffsetLocation(frame.offset).lineNumber, g.line);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLocation(String(frame.offset)).lineNumber; }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLocation(Object(frame.offset)).lineNumber; }, Error);

    assertThrowsInstanceOf(function () { frame.script.getOffsetLocation(-1).lineNumber; }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLocation(1000000).lineNumber; }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLocation(0.25).lineNumber; }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLocation(+Infinity).lineNumber; }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLocation(-Infinity).lineNumber; }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLocation(NaN).lineNumber; }, Error);

    assertThrowsInstanceOf(function () { frame.script.getOffsetLocation(false).lineNumber; }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLocation(true).lineNumber; }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLocation(undefined).lineNumber; }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLocation().lineNumber; }, Error);

    // We assume that at least one whole number between 0 and frame.offset is invalid.
    assertThrowsInstanceOf(
        function () {
            for (var i = 0; i < frame.offset; i++)
                frame.script.getOffsetLocation(i).lineNumber;
        },
        Error);

    hits++;
};

hits = 0;
g.eval("var line = new Error().lineNumber; debugger;");
