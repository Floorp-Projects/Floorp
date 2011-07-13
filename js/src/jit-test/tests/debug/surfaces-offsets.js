// Invalid offsets result in exceptions, not bogus results.

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var hits;
dbg.onDebuggerStatement = function (frame) {
    assertEq(frame.script.getOffsetLine(frame.offset), g.line);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLine(String(frame.offset)); }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLine(Object(frame.offset)); }, Error);

    assertThrowsInstanceOf(function () { frame.script.getOffsetLine(-1); }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLine(1000000); }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLine(0.25); }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLine(+Infinity); }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLine(-Infinity); }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLine(NaN); }, Error);

    assertThrowsInstanceOf(function () { frame.script.getOffsetLine(false); }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLine(true); }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLine(undefined); }, Error);
    assertThrowsInstanceOf(function () { frame.script.getOffsetLine(); }, Error);

    // We assume that at least one whole number between 0 and frame.offset is invalid.
    assertThrowsInstanceOf(
        function () {
            for (var i = 0; i < frame.offset; i++)
                frame.script.getOffsetLine(i);
        },
        Error);

    hits++;
};

hits = 0;
g.eval("var line = new Error().lineNumber; debugger;");
