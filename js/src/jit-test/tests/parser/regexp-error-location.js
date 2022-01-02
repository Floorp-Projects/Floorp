// Check that error location for RegExp points to the actual line/column inside
// the script instead of inside the pattern.
var line, column;
try {
  eval(`

  /aaa(/;
012345678;
`);
} catch (e) {
  line = e.lineNumber;
  column = e.columnNumber;
}

assertEq(line, 3);
assertEq(column, 6);
