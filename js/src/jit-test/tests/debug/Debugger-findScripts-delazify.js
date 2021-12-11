// |jit-test| skip-if: isLcovEnabled()

// findScript should try to avoid delazifying unnecessarily.

function newTestcase(code) {
  var g = newGlobal({newCompartment: true});
  var dbg = new Debugger();
  var gw = dbg.addDebuggee(g);

  var lines = code.split('\n');
  // Returns the line number of the line with "<= line".
  // + 1 for 1-origin.
  var line = lines.findIndex(x => x.includes("<= line")) + 1;

  g.eval(code);

  // Functions are relazified, and the top-level script is dropped.
  relazifyFunctions();

  return [dbg, g, line];
}

var url = thisFilename();
var dbg, g, line, scripts;

// If the specified line is inside the function body, only the function should
// be delazified.
[dbg, g, line] = newTestcase(`
function f1() {
}
function f2() {
// <= line
}
function f3() {
}
`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 1);
assertEq(scripts.map(s => s.displayName).sort().join(","), "f2");

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), false);
assertEq(g.eval(`isLazyFunction(f3)`), true);

// If the functions starts at the specified line, the function shouldn't be
// the first function to delazify.
[dbg, g, line] = newTestcase(`
function f1() {
}
function f2() {
}
function f3() { // <= line
}
function f4() {
}
`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), true);
assertEq(g.eval(`isLazyFunction(f4)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 1);
assertEq(scripts.map(s => s.displayName).sort().join(","), "f3");

assertEq(g.eval(`isLazyFunction(f1)`), true);
// f2 is delazified because f3 cannot be the first function to delazify.
assertEq(g.eval(`isLazyFunction(f2)`), false);
assertEq(g.eval(`isLazyFunction(f3)`), false);
assertEq(g.eval(`isLazyFunction(f4)`), true);

// Multiple functions in the specified line, and one of them starts before
// the specified line.
// All functions should be returned, and others shouldn't be delazified.
[dbg, g, line] = newTestcase(`
function f1() {}
function f2() {
} function f3() {} function f4() {} function f5() { // <= line
}
function f6() {}
`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), true);
assertEq(g.eval(`isLazyFunction(f4)`), true);
assertEq(g.eval(`isLazyFunction(f5)`), true);
assertEq(g.eval(`isLazyFunction(f6)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 4);
assertEq(scripts.map(s => s.displayName).sort().join(","), "f2,f3,f4,f5");

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), false);
assertEq(g.eval(`isLazyFunction(f3)`), false);
assertEq(g.eval(`isLazyFunction(f4)`), false);
assertEq(g.eval(`isLazyFunction(f5)`), false);
assertEq(g.eval(`isLazyFunction(f6)`), true);

// The same rule should apply to inner functions.
[dbg, g, line] = newTestcase(`
function f1() {}
function f2() {
  function g1() {
  }
  function g2() {
    function h1() {}
    function h2() {
    } function h3() {} function h4() {} function h5() { // <= line
    }
    function h6() {}

    return [h1, h2, h3, h4, h5, h6];
  }
  function g3() {
  }

  return [g1, g2, g3];
}
function f3() {}
`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 6);
assertEq(scripts.map(s => s.displayName).sort().join(","), "f2,g2,h2,h3,h4,h5");

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), false);
assertEq(g.eval(`isLazyFunction(f3)`), true);
g.eval(`var [g1, g2, g3] = f2();`);
assertEq(g.eval(`isLazyFunction(g1)`), true);
assertEq(g.eval(`isLazyFunction(g2)`), false);
assertEq(g.eval(`isLazyFunction(g3)`), true);
g.eval(`var [h1, h2, h3, h4, h5, h6] = g2();`);
assertEq(g.eval(`isLazyFunction(h1)`), true);
assertEq(g.eval(`isLazyFunction(h2)`), false);
assertEq(g.eval(`isLazyFunction(h3)`), false);
assertEq(g.eval(`isLazyFunction(h4)`), false);
assertEq(g.eval(`isLazyFunction(h5)`), false);
assertEq(g.eval(`isLazyFunction(h6)`), true);

// The same rule should apply to functions inside parameter expression.
[dbg, g, line] = newTestcase(`
function f1(
  a = function g1() {},
  b = function g2() {
  }, c = function g3() {}, d = function g4() { // <= line
  },
  e = function g5() {},
) {
  return [a, b, c, d, e];
}
`);

assertEq(g.eval(`isLazyFunction(f1)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 4);
assertEq(scripts.map(s => s.displayName).sort().join(","), "f1,g2,g3,g4");

assertEq(g.eval(`isLazyFunction(f1)`), false);

g.eval(`var [g1, g2, g3, g4, g5] = f1();`);

assertEq(g.eval(`isLazyFunction(g1)`), true);
assertEq(g.eval(`isLazyFunction(g2)`), false);
assertEq(g.eval(`isLazyFunction(g3)`), false);
assertEq(g.eval(`isLazyFunction(g4)`), false);
assertEq(g.eval(`isLazyFunction(g5)`), true);

// The same should apply to function inside method with computed property.
[dbg, g, line] = newTestcase(`
var f1, f2, f3;
var O = {
  [(f1 = () => 0, "m1")]() {
  },
  [(f2 = () => 0, "m2")](p2) { // <= line
  },
  [(f3 = () => 0, "m3")]() {
  },
}
`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(O.m2)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(O.m1)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), true);
assertEq(g.eval(`isLazyFunction(O.m3)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 2);
assertEq(scripts.map(s => s.displayName).sort().join(","), "f2,");
// Use parameterNames because displayName isn't set for computed property.
assertEq(scripts.map(s => `${s.displayName}(${s.parameterNames.join(",")})`)
         .sort().join(","), "f2(),undefined(p2)");

assertEq(g.eval(`isLazyFunction(f1)`), true);
// m1 is delazified because f2 cannot be the first function to delazify.
assertEq(g.eval(`isLazyFunction(O.m1)`), false);
assertEq(g.eval(`isLazyFunction(f2)`), false);
assertEq(g.eval(`isLazyFunction(O.m2)`), false);
assertEq(g.eval(`isLazyFunction(f3)`), true);
assertEq(g.eval(`isLazyFunction(O.m3)`), true);

[dbg, g, line] = newTestcase(`
var f1, f2, f3;
var O = {
  [(f1 = () => 0, "m1")]() {
  },
  [(f2 = () => 0 +
  1, "m2")](p2) { // <= line
  },
  [(f3 = () => 0, "m3")]() {
  },
}
`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(O.m1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(O.m2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), true);
assertEq(g.eval(`isLazyFunction(O.m3)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 2);
assertEq(scripts.map(s => `${s.displayName}(${s.parameterNames.join(",")})`)
         .sort().join(","), "f2(),undefined(p2)");

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(O.m1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), false);
assertEq(g.eval(`isLazyFunction(O.m2)`), false);
assertEq(g.eval(`isLazyFunction(f3)`), true);
assertEq(g.eval(`isLazyFunction(O.m3)`), true);

// Class constructor shouldn't be delazified even if methods match.
[dbg, g, line] = newTestcase(`
// Use variable to access across eval.
var C = class {
  constructor() {
  }
  m1() {}
  m2() {
  } m3() {} m4() { // <= line
  }
  m5() {}
}
`);

assertEq(g.eval(`isLazyFunction(C)`), true);
assertEq(g.eval(`isLazyFunction(C.prototype.m1)`), true);
assertEq(g.eval(`isLazyFunction(C.prototype.m2)`), true);
assertEq(g.eval(`isLazyFunction(C.prototype.m3)`), true);
assertEq(g.eval(`isLazyFunction(C.prototype.m4)`), true);
assertEq(g.eval(`isLazyFunction(C.prototype.m5)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 3);
assertEq(scripts.map(s => s.displayName).sort().join(","), "m2,m3,m4");

assertEq(g.eval(`isLazyFunction(C)`), true);
assertEq(g.eval(`isLazyFunction(C.prototype.m1)`), true);
assertEq(g.eval(`isLazyFunction(C.prototype.m2)`), false);
assertEq(g.eval(`isLazyFunction(C.prototype.m3)`), false);
assertEq(g.eval(`isLazyFunction(C.prototype.m4)`), false);
assertEq(g.eval(`isLazyFunction(C.prototype.m5)`), true);

// If the line is placed before sourceStart, the function shouldn't match,
// and the function shouldn't be delazified.
[dbg, g, line] = newTestcase(`
function f1() {
}
function f2() {
}
function
// <= line
f3() {
}
function f4() {
}
`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), true);
assertEq(g.eval(`isLazyFunction(f4)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 0);

assertEq(g.eval(`isLazyFunction(f1)`), true);
// f2 is delazified because it's the first function before the specified line.
assertEq(g.eval(`isLazyFunction(f2)`), false);
assertEq(g.eval(`isLazyFunction(f3)`), true);
assertEq(g.eval(`isLazyFunction(f4)`), true);

[dbg, g, line] = newTestcase(`
function f1() {
}
function f2() {
}
function f3
// <= line
() {
}
function f4() {
}
`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), true);
assertEq(g.eval(`isLazyFunction(f4)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 0);

assertEq(g.eval(`isLazyFunction(f1)`), true);
// f2 is delazified because it's the first function before the specified line.
assertEq(g.eval(`isLazyFunction(f2)`), false);
assertEq(g.eval(`isLazyFunction(f3)`), true);
assertEq(g.eval(`isLazyFunction(f4)`), true);

[dbg, g, line] = newTestcase(`
function f1() {
}
function f2() {
}
function f3
( // <= line
) {
}
function f4() {
}
`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), true);
assertEq(g.eval(`isLazyFunction(f4)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 1);
assertEq(scripts.map(s => s.displayName).sort().join(","), "f3");

assertEq(g.eval(`isLazyFunction(f1)`), true);
// f2 is delazified because it's the first function _before_ the specified line.
assertEq(g.eval(`isLazyFunction(f2)`), false);
assertEq(g.eval(`isLazyFunction(f3)`), false);
assertEq(g.eval(`isLazyFunction(f4)`), true);

[dbg, g, line] = newTestcase(`
function f1() {
}
function f2() {
}
function f3
(
// <= line
) {
}
function f4() {
}
`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), true);
assertEq(g.eval(`isLazyFunction(f4)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 1);
assertEq(scripts.map(s => s.displayName).sort().join(","), "f3");

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), false);
assertEq(g.eval(`isLazyFunction(f4)`), true);

// If the specified line is the next line after the function ends,
// nothing should match, but the function should be delazified.
[dbg, g, line] = newTestcase(`
function f1() {
}
function f2() {
}
// <= line
function f3() {
}
`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 0);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), false);
assertEq(g.eval(`isLazyFunction(f3)`), true);

// The matching non-lazy script should prevent the previous function's
// delazification.
[dbg, g, line] = newTestcase(`
function f1() {
}
function f2() {
}
function f3() {
  // <= line
}
function f4() {
}
`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), true);
assertEq(g.eval(`isLazyFunction(f4)`), true);

// Delazify f3.
g.eval(`f3()`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), false);
assertEq(g.eval(`isLazyFunction(f4)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 1);
assertEq(scripts.map(s => s.displayName).sort().join(","), "f3");

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), false);
assertEq(g.eval(`isLazyFunction(f4)`), true);

// The non-matching non-lazy script should prevent the previous function's
// delazification.
[dbg, g, line] = newTestcase(`
function f1() {
}
function f2() {
}
function f3() {
}
// <= line
function f4() {
}
`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), true);
assertEq(g.eval(`isLazyFunction(f4)`), true);

// Delazify f3.
g.eval(`f3()`);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), false);
assertEq(g.eval(`isLazyFunction(f4)`), true);

scripts = dbg.findScripts({url, line});
assertEq(scripts.length, 0);

assertEq(g.eval(`isLazyFunction(f1)`), true);
assertEq(g.eval(`isLazyFunction(f2)`), true);
assertEq(g.eval(`isLazyFunction(f3)`), false);
assertEq(g.eval(`isLazyFunction(f4)`), true);
