function f1(foo, bar) foo + bar;
assertEq(f1.toString(), "function f1(foo, bar) foo + bar");
assertEq(f1.toString(), f1.toSource());
assertEq(decompileFunction(f1), f1.toString());
// No semicolon on purpose
function f2(foo, bar) foo + bar
assertEq(f2.toString(), "function f2(foo, bar) foo + bar");
assertEq(f2.toString(), f2.toSource());
var f3 = function (foo, bar) foo + bar;
assertEq(f3.toSource(), "(function (foo, bar) foo + bar)");
assertEq(f3.toString(), "function (foo, bar) foo + bar");
// No semicolon on purpose
var f4 = function (foo, bar) foo + bar
assertEq(f4.toSource(), "(function (foo, bar) foo + bar)");
assertEq(f4.toString(), "function (foo, bar) foo + bar");
var f5 = function (foo, bar) foo + bar   ;
assertEq(f5.toSource(), "(function (foo, bar) foo + bar)");
assertEq(f5.toString(), "function (foo, bar) foo + bar");
var f6 = function (foo, bar) foo + bar; var a = 42
assertEq(f6.toSource(), "(function (foo, bar) foo + bar)");
assertEq(f6.toString(), "function (foo, bar) foo + bar");
var f7 = function (foo, bar) foo + bar + '\
long\
string\
test\
'
// a comment followed by some space


assertEq(f7.toString(), "function (foo, bar) foo + bar + '\\\nlong\\\nstring\\\ntest\\\n'");
assertEq(f7.toSource(), "(" + f7.toString() + ")");
