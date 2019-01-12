// Debugger and debuggees must be in different compartments.

load(libdir + "asserts.js");

function testConstructor() {
    var g = newGlobal({sameCompartmentAs: this});
    assertTypeErrorMessage(() => new Debugger(g),
                           "Debugger: argument must be an object from a different compartment");
}
testConstructor();

function testAddDebuggee() {
    var g = newGlobal({sameCompartmentAs: this});
    var dbg = new Debugger();
    assertTypeErrorMessage(() => dbg.addDebuggee(this),
                           "debugger and debuggee must be in different compartments");
}
testAddDebuggee();

function testAddAllGlobalsAsDebuggees() {
    var g1 = newGlobal({sameCompartmentAs: this});
    var g2 = newGlobal({newCompartment: true});
    var g3 = newGlobal({sameCompartmentAs: g2});
    var g4 = newGlobal({newCompartment: true, sameZoneAs: this});
    var dbg = new Debugger();
    dbg.addAllGlobalsAsDebuggees();
    assertEq(dbg.hasDebuggee(g1), false);
    assertEq(dbg.hasDebuggee(g2), true);
    assertEq(dbg.hasDebuggee(g3), true);
    assertEq(dbg.hasDebuggee(g4), true);
}
testAddAllGlobalsAsDebuggees();
