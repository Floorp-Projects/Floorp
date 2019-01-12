// Ensure nuking happens on a single target realm instead of compartment.

var g1 = newGlobal({newCompartment: true});
var g2 = newGlobal({sameCompartmentAs: g1});
g2.other = g1;

var o1 = g1.Math;
var o2 = g2.Math;

g1.nukeAllCCWs();

// o1 is now dead.
ex = null;
try {
    assertEq(o1.abs(1), 1);
} catch (e) {
    ex = e;
}
assertEq(ex.toString().includes("dead object"), true);

// o2 still works.
assertEq(o2.abs(1), 1);

// g2 can still access g1 because they're same-compartment.
assertEq(g2.evaluate("other.Math.abs(-2)"), 2);

// Attempting to create a new wrapper targeting nuked realm g1 should return a
// dead wrapper now. Note that we can't use g1 directly because that's now a
// dead object, so we try to get to g1 via g2.
ex = null;
try {
    g2.other.toString();
} catch (e) {
    ex = e;
}
assertEq(ex.toString().includes("dead object"), true);

// Nuke g2 too. We have nuked all realms in its compartment so we should now
// throw if we try to create a new outgoing wrapper.
g2.evaluate("(" + function() {
    nukeAllCCWs();
    var ex = null;
    try {
        newGlobal({newCompartment: true}).Array();
    } catch (e) {
        ex = e;
    }
    assertEq(ex.toString().includes('dead object'), true);
} + ")()");
