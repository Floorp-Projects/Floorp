// env.find throws a TypeError if the argument is not an identifier.

load(libdir + "asserts.js");

var g = newGlobal('new-compartment');
var dbg = Debugger(g);
var hits = 0;
g.h = function () {
    var env = dbg.getNewestFrame().environment;
    assertThrowsInstanceOf(function () { env.find(); }, TypeError);
    assertThrowsInstanceOf(function () { env.find(""); }, TypeError);
    assertThrowsInstanceOf(function () { env.find(" "); }, TypeError);
    assertThrowsInstanceOf(function () { env.find(0); }, TypeError);
    assertThrowsInstanceOf(function () { env.find("0"); }, TypeError);
    assertThrowsInstanceOf(function () { env.find("0xc"); }, TypeError);
    assertThrowsInstanceOf(function () { env.find("Anna Karenina"); }, TypeError);
    hits++;
};
g.eval("h();");
g.eval("with ([1]) h();");
assertEq(hits, 2);
