// |jit-test| skip-if: isLcovEnabled()

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);

g.eval(`
function f(
  a = (
    b = (
      c = function g() {
      },
    ) => {
    },
    d = (
      e = (
        f = (
        ) => {
        },
      ) => {
      },
    ) => {
    },
  ) => {
  },
) {
}
`);

// Debugger shouldn't find ghost functions.
var allScripts = dbg.findScripts();
assertEq(allScripts.filter(s => s.startLine == 2).length, 1); // function f
assertEq(allScripts.filter(s => s.startLine == 3).length, 1); // a = ...
assertEq(allScripts.filter(s => s.startLine == 4).length, 1); // b = ...
assertEq(allScripts.filter(s => s.startLine == 5).length, 1); // function g
assertEq(allScripts.filter(s => s.startLine == 9).length, 1); // d = ...
assertEq(allScripts.filter(s => s.startLine == 10).length, 1); // e = ...
assertEq(allScripts.filter(s => s.startLine == 11).length, 1); // f = ...
