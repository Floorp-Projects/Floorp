load(libdir + "assert-offset-columns.js");

// getColumnOffsets correctly places the various parts of a ForStatement.
assertOffsetColumns(
    "function f(n) { for (var i = 0; i < n; ++i) hits.push('.'); hits.push('!'); }",
    "                         ^      ^      ^    ^               ^               ^",
    "0 1 3 . 2 1 3 . 2 1 3 . 2 1 4 ! 5",
);

// getColumnOffsets correctly places multiple variable declarations.
assertOffsetColumns(
    "function f(n){var w0,x1=3,y2=4,z3=9}",
    "                     ^    ^    ^   ^",
);

// getColumnOffsets correctly places comma separated expressions.
assertOffsetColumns(
    "function f(n){print(n),print(n),print(n)}",
    "              ^        ^        ^       ^",
);

// getColumnOffsets correctly places object properties.
assertOffsetColumns(
    // Should hit each property in the object.
    "function f(n){var o={a:1,b:2,c:3}}",
    "                  ^  ^   ^   ^   ^",
);

// getColumnOffsets correctly places array properties.
assertOffsetColumns(
    // Should hit each item in the array.
    "function f(n){var a=[1,2,n]}",
    "                  ^  ^ ^ ^ ^",
);

// getColumnOffsets correctly places function calls.
assertOffsetColumns(
    "function ppppp() { return 1; }\n" +
    "function f(){ 1 && ppppp(ppppp()) && new Error() }",
    "              ^    ^     ^           ^           ^",
    "0 2 1 3 4",
);

// getColumnOffsets correctly places the various parts of a SwitchStatement.
assertOffsetColumns(
    "function f(n) { switch(n) { default: print(n); } }",
    "                ^                    ^           ^",
);

// getColumnOffsets correctly places the various parts of a BreakStatement.
assertOffsetColumns(
    "function f(n) { do { print(n); break; } while(false); }",
    "                ^    ^         ^                      ^",
);

// getColumnOffsets correctly places the various parts of a ContinueStatement.
assertOffsetColumns(
    "function f(n) { do { print(n); continue; } while(false); }",
    "                ^    ^         ^                         ^",
);

// getColumnOffsets correctly places the various parts of a WithStatement.
assertOffsetColumns(
    "function f(n) { with({}) { print(n); } }",
    "                ^          ^           ^",
);

// getColumnOffsets correctly places the various parts of a IfStatement.
assertOffsetColumns(
    "function f(n) { if (n == 3) print(n); }",
    "                ^           ^         ^",
);

// getColumnOffsets correctly places the various parts of a IfStatement
// with an if/else
assertOffsetColumns(
    "function f(n) { if (n == 2); else if (n === 3) print(n); }",
    "                ^                 ^            ^         ^",
);

// getColumnOffsets correctly places the various parts of a DoWhileStatement.
assertOffsetColumns(
    "function f(n) { do { print(n); } while(false); }",
    "                ^    ^                         ^",
);
