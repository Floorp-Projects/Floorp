
// Simple tests for evaluate's "lineNumber" option.

load(libdir + 'asserts.js');

const maxLine = Math.pow(2,32) - 1;

assertEq(evaluate("saveStack().line"), 1);
assertEq(evaluate("saveStack().line", { lineNumber: maxLine }), maxLine);
assertEq(evaluate("\nsaveStack().line"), 2);
assertEq(evaluate("\nsaveStack().line", { lineNumber: 1000 }), 1001);
assertThrowsInstanceOf(() => evaluate("\nsaveStack().line", { lineNumber: maxLine }),
                       RangeError);

if (helperThreadCount() > 0) {
  offThreadCompileScript("saveStack().line");
  assertEq(runOffThreadScript(), 1);

  offThreadCompileScript("saveStack().line", { lineNumber: maxLine });
  assertEq(runOffThreadScript(), maxLine);

  offThreadCompileScript("\nsaveStack().line");
  assertEq(runOffThreadScript(), 2);

  offThreadCompileScript("\nsaveStack().line", { lineNumber: 1000 });
  assertEq(runOffThreadScript(), 1001);

  offThreadCompileScript("\nsaveStack().line", { lineNumber: maxLine });
  assertThrowsInstanceOf(runOffThreadScript, RangeError);
}
