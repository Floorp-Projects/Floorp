// Simple tests for evaluate's "columnNumber" option.

load(libdir + 'asserts.js');

assertEq(evaluate("saveStack().column"), 0);
assertEq(evaluate("saveStack().column", { columnNumber: 1729 }), 1729);
assertEq(evaluate("\nsaveStack().column", { columnNumber: 1729 }), 0);
assertEq(evaluate("saveStack().column", { columnNumber: "42" }), 42);
assertThrowsInstanceOf(() => evaluate("saveStack().column", { columnNumber: -10 }),
                       RangeError);
assertThrowsInstanceOf(() => evaluate("saveStack().column", { columnNumber: Math.pow(2,30) }),
                       RangeError);

if (helperThreadCount() > 0) {
  print("offThreadCompileScript 1");
  offThreadCompileScript("saveStack().column", { columnNumber: -10 });
  assertThrowsInstanceOf(runOffThreadScript, RangeError);

  print("offThreadCompileScript 2");
  offThreadCompileScript("saveStack().column", { columnNumber: Math.pow(2,30) });
  assertThrowsInstanceOf(runOffThreadScript, RangeError);

  print("offThreadCompileScript 3");
  offThreadCompileScript("saveStack().column", { columnNumber: 10000 });
  assertEq(runOffThreadScript(), 10000);
}

// Check handling of columns near the limit of our ability to represent them.
// (This is hardly thorough, but since web content can't set column numbers,
// it's probably not worth it to be thorough.)
const maxColumn = Math.pow(2, 22) - 1;
assertEq(evaluate("saveStack().column", { columnNumber: maxColumn }),
         maxColumn);
assertEq(evaluate("saveStack().column", { columnNumber: maxColumn + 1 }),
         0);

// Check the 'silently zero' behavior when we reach the limit of the srcnotes
// column encoding.
assertEq(evaluate(" saveStack().column", { columnNumber: maxColumn }),
         0);
