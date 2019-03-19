// |reftest| skip-if(!xulRuntime.shell)
function test() {

// Source location information
var withoutFileOrLine = Reflect.parse("42");
var withFile = Reflect.parse("42", {source:"foo.js"});
var withFileAndLine = Reflect.parse("42", {source:"foo.js", line:111});

Pattern({ source: null, start: { line: 1, column: 0 }, end: { line: 1, column: 2 } }).match(withoutFileOrLine.loc);
Pattern({ source: "foo.js", start: { line: 1, column: 0 }, end: { line: 1, column: 2 } }).match(withFile.loc);
Pattern({ source: "foo.js", start: { line: 111, column: 0 }, end: { line: 111, column: 2 } }).match(withFileAndLine.loc);

var withoutFileOrLine2 = Reflect.parse("foo +\nbar");
var withFile2 = Reflect.parse("foo +\nbar", {source:"foo.js"});
var withFileAndLine2 = Reflect.parse("foo +\nbar", {source:"foo.js", line:111});

Pattern({ source: null, start: { line: 1, column: 0 }, end: { line: 2, column: 3 } }).match(withoutFileOrLine2.loc);
Pattern({ source: "foo.js", start: { line: 1, column: 0 }, end: { line: 2, column: 3 } }).match(withFile2.loc);
Pattern({ source: "foo.js", start: { line: 111, column: 0 }, end: { line: 112, column: 3 } }).match(withFileAndLine2.loc);

var nested = Reflect.parse("(-b + sqrt(sqr(b) - 4 * a * c)) / (2 * a)", {source:"quad.js"});
var fourAC = nested.body[0].expression.left.right.arguments[0].right;

Pattern({ source: "quad.js", start: { line: 1, column: 20 }, end: { line: 1, column: 29 } }).match(fourAC.loc);

// No source location

assertEq("loc" in Reflect.parse("42", {loc:false}), false);
program([exprStmt(lit(42))]).assert(Reflect.parse("42", {loc:false}));

}

runtest(test);
