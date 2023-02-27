// Test `name` and `displayName` getters on a bound function Debugger.Object.

var g = newGlobal({newCompartment: true});
var hits = 0;
var dbg = new Debugger(g);

var nameResults = [];
var displayNameResults = [];
dbg.onDebuggerStatement = function (frame) {
    nameResults.push(frame.arguments[0].name);
    displayNameResults.push(frame.arguments[0].displayName);
};
g.eval(`
function f(val) { debugger; }

function foo() {};
let bound1 = foo.bind(null);
let bound2 = bound1.bind();
let bound3 = bound2.bind();
f(bound1);
f(bound2);
f(bound3);

// Change bound1.name to "bar".
Object.defineProperty(bound1, "name", {value: "bar", configurable: true, writable: true});
f(bound1);

// Change bound1.name to 123. Not a string so now "bound" will be returned.
bound1.name = 123;
f(bound1);

// Delete bound2.name so now "bound" will be used.
delete bound2.name;
f(bound2);

// Binding a native function.
f(print.bind().bind().bind());
`)

for (let res of [nameResults, displayNameResults]) {
    assertEq(res.length, 7);
    assertEq(res[0], "bound foo");
    assertEq(res[1], "bound bound foo");
    assertEq(res[2], "bound bound bound foo");
    assertEq(res[3], "bar");
    assertEq(res[4], "bound");
    assertEq(res[5], "bound");
    assertEq(res[6], "bound bound bound print");
}
