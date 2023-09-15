load(libdir + "assert-offset-columns.js");

// getColumnOffsets correctly places the various parts of a ForStatement.
assertOffsetColumns(
  "function f(n) { for (var i = 0; i < n; ++i) hits.push('.'); hits.push('!'); }",
  "                             ^  ^   ^  ^    ^    ^          ^    ^          ^",
  //                            0  1   2  3    4    5          6    7          8
  "0 1 2 4 5 . 3 1 2 4 5 . 3 1 2 4 5 . 3 1 2 6 7 ! 8"
);

// getColumnOffsets correctly places multiple variable declarations.
assertOffsetColumns(
  "function f(n){var w0,x1=3,y2=4,z3=9}",
  "                        ^    ^    ^^"
);

// getColumnOffsets correctly places comma separated expressions.
assertOffsetColumns(
  "function f(n){print(n),print(n),print(n)}",
  "              ^    ^^  ^     ^  ^     ^ ^",
  //             0    12  3     4  5     6 7
  "0 2 1 3 4 3 5 6 5 7"
);

// getColumnOffsets correctly places object properties.
assertOffsetColumns(
  // Should hit each property in the object.
  "function f(n){var o={a:1,b:2,c:3}}",
  "                    ^^   ^   ^   ^"
);

// getColumnOffsets correctly places array properties.
assertOffsetColumns(
  // Should hit each item in the array.
  "function f(n){var a=[1,2,n]}",
  "                    ^    ^ ^"
);

// getColumnOffsets correctly places function calls.
assertOffsetColumns(
  "function ppppp() { return 1; }\n" +
  "function f(){ 1 && ppppp(ppppp()) && new Error() }",
  "              ^    ^     ^           ^           ^",
  //             0    1     2           3           4
  "0 2 1 3 4"
);

// getColumnOffsets correctly places the various parts of a SwitchStatement.
assertOffsetColumns(
  "function f(n) { switch(n) { default: print(n); } }",
  "                       ^             ^    ^^     ^",
  //                      0             1    23     4
  "0 1 3 2 4"
);

// getColumnOffsets correctly places the various parts of a BreakStatement.
assertOffsetColumns(
  "function f(n) { do { print(n); if (n === 3) { break; } } while(false); }",
  "                ^    ^    ^^       ^          ^                ^       ^",
  //               0    1    23       4          5                6       7
  "0 1 3 2 4 5 7"
);

// If the loop condition is unreachable, we currently don't report its offset.
assertOffsetColumns(
  "function f(n) { do { print(n); break; } while(false); }",
  "                ^    ^    ^^   ^                      ^",
  //               0    1    23   4                      5
  "0 1 3 2 4 5"
);

// getColumnOffsets correctly places the various parts of a ContinueStatement.
assertOffsetColumns(
  "function f(n) { do { print(n); continue; } while(false); }",
  "                ^    ^    ^^   ^                 ^       ^",
  //               0    1    23   4                 5       6
  "0 1 3 2 4 5 6"
);

// getColumnOffsets correctly places the various parts of a WithStatement.
assertOffsetColumns(
  "function f(n) { with({}) { print(n); } }",
  "                     ^     ^    ^^     ^",
  //                    0     1    23     4
  "0 1 3 2 4"
);

// getColumnOffsets correctly places the various parts of a IfStatement.
assertOffsetColumns(
  "function f(n) { if (n == 3) print(n); }",
  "                    ^       ^    ^^   ^",
  //                   0       1    23   4
  "0 1 3 2 4"
);

// getColumnOffsets correctly places the various parts of a IfStatement
// with an if/else
assertOffsetColumns(
  "function f(n) { if (n == 2); else if (n === 3) print(n); }",
  "                    ^                 ^        ^    ^^   ^",
  //                   0                 1        2    34   5
  "0 1 2 4 3 5"
);

// getColumnOffsets correctly places the various parts of a DoWhileStatement.
assertOffsetColumns(
  "function f(n) { do { print(n); } while(false); }",
  "                ^    ^    ^^           ^       ^",
  //               0    1    23           4       5
  "0 1 3 2 4 5"
);

// getColumnOffsets correctly places the part of normal ::Dot node with identifier root.
assertOffsetColumns(
  "var args = [];\n" +
  "var obj = { base: { a(){ return { b(){} }; } } };\n" +
  "function f(n) { obj.base.a().b(...args); }",
  "                ^        ^   ^ ^  ^      ^",
  //               0        1   2 3  4      5
  "0 1 4 2 5"
);

// getColumnOffsets correctly places the part of normal ::Dot node with "this" root.
assertOffsetColumns(
  "var args = [];\n" +
  "var obj = { base: { a(){ return { b(){} }; } } };\n" +
  "var f = function() { this.base.a().b(...args);  }.bind(obj);",
  "                     ^         ^   ^ ^  ^       ^",
  //                    0         1   2 3  4       5
  "0 1 4 2 5"
);

// getColumnOffsets correctly places the part of normal ::Dot node with "super" base.
assertOffsetColumns(
  "var args = [];\n" +
  "var obj = { base: { a(){ return { b(){} }; } } };\n" +
  "var f = { __proto__: obj, f(n) { super.base.a().b(...args); } }.f;",
  "                                 ^          ^   ^ ^  ^      ^",
  //                                0          1   2 3  4      5
  "0 1 4 2 5"
);

// getColumnOffsets correctly places the part of normal ::Dot node with other base.
assertOffsetColumns(
  "var args = [];\n" +
  "var obj = { base: { a(){ return { b(){} }; } } };\n" +
  "function f(n) { (0, obj).base.a().b(...args); }",
  "                 ^  ^         ^   ^ ^  ^      ^",
  //                0  1         2   3 4  5      6
  "0 1 2 5 3 6"
);

// getColumnOffsets correctly places the part of folded ::Elem node.
assertOffsetColumns(
  "var args = [];\n" +
  "var obj = { base: { a(){ return { b(){} }; } } };\n" +
  // Constant folding makes the static string behave like a dot access.
  "function f(n) { obj.base['a']()['b'](...args); }",
  "                ^        ^      ^    ^  ^      ^",
  //               0        1      2    3  4      5
  "0 1 4 2 5"
);

// getColumnOffsets correctly places the part of computed ::Elem node.
assertOffsetColumns(
  "var args = [], a = 'a', b = 'b';\n" +
  "var obj = { base: { a(){ return { b(){} }; } } };\n" +
  "function f(n) { obj.base[a]()[b](...args); }",
  "                ^          ^    ^^  ^      ^",
  //               0          1    23  4      5
  "0 1 4 2 5"
);

// getColumnOffsets correctly places the evaluation of ...args when
// OptimizeSpreadCall fails.
assertOffsetColumns(
  "var args = [,];\n" +
  "var obj = { base: { a(){ return { b(){} }; } } };\n" +
  "function f(n) { obj.base.a().b(...args); }",
  "                ^        ^   ^ ^  ^      ^",
  //               0        1   2 3  4      5
  "0 1 4 3 2 5"
);
