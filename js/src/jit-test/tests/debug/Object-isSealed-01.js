// Basic tests for Debugger.Object.prototype.{isSealed,isFrozen,isExtensible}.

var g = newGlobal("new-compartment");
var dbg = new Debugger;
var gw = dbg.addDebuggee(g);

function test(code) {
    g.eval("x = (" + code + ");");
    var xw = gw.getOwnPropertyDescriptor("x").value;

    function check() {
	assertEq(xw.isSealed(), g.Object.isSealed(g.x));
	assertEq(xw.isFrozen(), g.Object.isFrozen(g.x));
	assertEq(xw.isExtensible(), g.Object.isExtensible(g.x));
    }

    check();
    g.Object.preventExtensions(g.x);
    check();
    g.Object.seal(g.x);
    check();
    g.Object.freeze(g.x);
    check();
}

test("{}");
test("{a: [1]}");
test("Object.create(null, {x: {value: 3}, y: {get: Math.min}})");
test("[]");
test("[,,,,,]");
test("[0, 1, 2]");
test("this");  // This freezes g, so insert new tests before this line.
