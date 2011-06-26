// Invalid offsets result in exceptions, not bogus results.

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
var dbg = Debug(g);
var hits;
dbg.hooks = {
    debuggerHandler: function (frame) {
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
	hits++;
    }
};

hits = 0;
g.eval("var line = new Error().lineNumber; debugger;");
