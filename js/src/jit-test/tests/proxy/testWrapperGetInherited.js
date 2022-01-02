// Getting a property O.X, inherited from a transparent cross-compartment wrapper W
// that wraps a Proxy P.

var g = newGlobal();
var target = {}
var P = new Proxy(target, {
    get(t, id, r) {
        assertEq(t, target);
        assertEq(id, "X");
        assertEq(r, wO);
        return "vega";
    }
});

g.W = P;
g.eval("var O = Object.create(W);");
var wO = g.O;
assertEq(g.eval("O.X"), "vega");
