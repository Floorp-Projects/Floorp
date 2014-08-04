// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// This test case is weird in the sense the actual work happens at the eval
// at the end. If template strings are not enabled, the test cases would throw
// a syntax error and we'd have failure reported. To avoid that, the entire
// test case is commented out and is part of a function. We use toString to
// get the string version, obtain the actual lines to run, and then use eval to
// do the actual evaluation.

function syntaxError (script) {
    try {
        Function(script);
    } catch (e) {
        if (e.name === "SyntaxError") {
            return;
        }
    }
    throw new Error('Expected syntax error: ' + script);
}

// function definitions
function check(actual, expected) {
    assertEq(actual.length, expected.length);
    for (var i = 0; i < expected.length; i++)
        assertEq(actual[i], expected[i]);
}

function cooked(cs) { return cs; }
function raw(cs) { return cs.raw; }
function args(cs, ...rest) { return rest; }


// TEST BEGIN

// Literals
check(raw``, [""]);
check(raw`${4}a`, ["","a"]);
check(raw`${4}`, ["",""]);
check(raw`a${4}`, ["a",""]);
check(raw`a${4}b`, ["a","b"]);
check(raw`a${4}b${3}`, ["a","b",""]);
check(raw`a${4}b${3}c`, ["a","b","c"]);
check(raw`a${4}${3}c`, ["a","","c"]);
check(raw`${4}${3}`, ["","",""]);
check(raw`${4}\r\r${3}`, ["","\\r\\r",""]);
check(raw`${4}${3}c`, ["","","c"]);
check(raw`zyx${4}wvut${3}c`, ["zyx","wvut","c"]);
check(raw`zyx${4}wvut${3}\r\n`, ["zyx","wvut","\\r\\n"]);

check(raw`hey`, ["hey"]);
check(raw`he\r\ny`, ["he\\r\\ny"]);
check(raw`he\ry`, ["he\\ry"]);
check(raw`he\r\ry`, ["he\\r\\ry"]);
check(raw`he\ny`, ["he\\ny"]);
check(raw`he\n\ny`, ["he\\n\\ny"]);

check(cooked`hey`, ["hey"]);
check(cooked`he\r\ny`, ["he\r\ny"]);
check(cooked`he\ry`, ["he\ry"]);
check(cooked`he\r\ry`, ["he\r\ry"]);
check(cooked`he\ny`, ["he\ny"]);
check(cooked`he\n\ny`, ["he\n\ny"]);

check(eval("raw`\r`"), ["\n"]);
check(eval("raw`\r\n`"), ["\n"]);
check(eval("raw`\r\r\n`"), ["\n\n"]);
check(eval("raw`he\r\ny`"), ["he\ny"]);
check(eval("raw`he\ry`"), ["he\ny"]);
check(eval("raw`he\r\ry`"), ["he\n\ny"]);
check(eval("raw`he\r\r\ny`"), ["he\n\ny"]);


check(eval("cooked`\r`"), ["\n"]);
check(eval("cooked`\r\n`"), ["\n"]);
check(eval("cooked`\r\r\n`"), ["\n\n"]);
check(eval("cooked`he\r\ny`"), ["he\ny"]);
check(eval("cooked`he\ry`"), ["he\ny"]);
check(eval("cooked`he\r\ry`"), ["he\n\ny"]);
check(eval("cooked`he\r\r\ny`"), ["he\n\ny"]);

// Expressions
check(args`hey${"there"}now`, ["there"]);
check(args`hey${4}now`, [4]);
check(args`hey${4}`, [4]);
check(args`${4}`, [4]);
check(args`${4}${5}`, [4,5]);
check(args`a${4}${5}`, [4,5]);
check(args`a${4}b${5}`, [4,5]);
check(args`a${4}b${5}c`, [4,5]);
check(args`${4}b${5}c`, [4,5]);
check(args`${4}${5}c`, [4,5]);

var a = 10;
var b = 15;
check(args`${4 + a}${5 + b}c`, [14,20]);
check(args`${4 + a}${a + b}c`, [14,25]);
check(args`${b + a}${5 + b}c`, [25,20]);
check(args`${4 + a}${a + b}c${"a"}`, [14,25,"a"]);
check(args`a${"b"}${"c"}${"d"}`, ["b","c","d"]);
check(args`a${"b"}${"c"}${a + b}`, ["b","c",25]);
check(args`a${"b"}`, ["b"]);

// Expressions - complex substitutions
check(args`${`hey ${b + a} there`}${5 + b}c`, ["hey 25 there",20]);
check(args`${`hey ${`my ${b + a} good`} there`}${5 + b}c`, ["hey my 25 good there",20]);

syntaxError("args`${}`");
syntaxError("args`${`");
syntaxError("args`${\\n}`");
syntaxError("args`${yield 0}`");
syntaxError("args`");
syntaxError("args`$");
syntaxError("args`${");
syntaxError("args.``");

// Template substitution tests in the context of tagged templates
// Extra whitespace inside a template substitution is ignored.
check(args`a${
0
}`, [0]);

// Extra whitespace between tag and template is ignored
check(args
`a
${
0
}`, [0]);


check(args`${5}${ // Comments work in template substitutions.
// Even comments that look like code:
// 0}`, "FAIL");   /* NOTE: This whole line is a comment.
0}`, [5,0]);

check(args // Comments work in template substitutions.
// Even comments that look like code:
// 0}`, "FAIL");   /* NOTE: This whole line is a comment.
`${5}${0}`, [5,0]);


// Template substitutions are expressions, not statements.
syntaxError("args`${0;}`");
check(args`${
    function f() {
        return "ok";
    }()
}`, ["ok"]);

// Template substitutions can have side effects.
var x = 0;
check(args`${x += 1}`, [1]);
assertEq(x, 1);

// The production for a template substitution is Expression, not
// AssignmentExpression.
x = 0;
check(args`${++x, "o"}k`, ["o"]);
assertEq(x, 1);


// --> is not a comment inside a template.
check(cooked`
--> this is text
`, ["\n--> this is text\n"]);

// reentrancy
function f(n) {
    if (n === 0)
        return "";
    var res = args`${n}${f(n - 1)}`;
    return res[0] + res[1] + "";
}
assertEq(f(9), "987654321");

// Template string substitutions in generator functions can yield.
function* g() {
    var res = args`${yield 1} ${yield 2}`;
    return res[0] + res[1] + "";
}

var it = g();
var next = it.next();
assertEq(next.done, false);
assertEq(next.value, 1);
next = it.next("hello");
assertEq(next.done, false);
assertEq(next.value, 2);
next = it.next("world");
assertEq(next.done, true);
assertEq(next.value, "helloworld");

// undefined
assertEq(args`${void 0}`[0] + "", "undefined");
assertEq(args`${Object.doesNotHaveThisProperty}`[0] + "", "undefined");

var callSiteObj = [];
callSiteObj[0] = cooked`aa${4}bb`;
for (var i = 1; i < 3; i++)
    callSiteObj[i] = cooked`aa${4}bb`;

// Same call site object behavior
assertEq(callSiteObj[1], callSiteObj[2]);
assertEq(callSiteObj[0] !== callSiteObj[1], true);
assertEq("raw" in callSiteObj[0], true);

// Array length
assertEq(callSiteObj[0].raw.length, 2);
assertEq(callSiteObj[0].length, 2);

// Frozen objects
assertEq(Object.isFrozen(callSiteObj[0]), true);
assertEq(Object.isFrozen(callSiteObj[0].raw), true);

// Raw not enumerable
assertEq(callSiteObj[0].propertyIsEnumerable(callSiteObj[0].raw), false);

// Allow call syntax
check(new ((cs, sub) => function(){ return sub }) `${[1, 2, 3]}`, [1,2,3]);

var a = [];
function test() {
    var x = callSite => callSite;
    for (var i = 0; i < 2; i++)
        a[i] = eval("x``");
}
test();
assertEq(a[0] !== a[1], true);

// Test that |obj.method`template`| works
var newObj = {
    methodRetThis : function () {
        return this;
    },
    methodRetCooked : function (a) {
        return a;
    },
    methodRetRaw : function (a) {
        return a.raw;
    },
    methodRetArgs : function (a, ...args) {
        return args;
    }
}

assertEq(newObj.methodRetThis`abc${4}`, newObj);
check(newObj.methodRetCooked`abc${4}\r`, ["abc","\r"]);
check(eval("newObj.methodRetCooked`abc${4}\r`"), ["abc","\n"]);
check(newObj.methodRetRaw`abc${4}\r`, ["abc","\\r"]);
check(eval("newObj.methodRetRaw`abc${4}\r`"), ["abc","\n"]);
check(eval("newObj.methodRetArgs`abc${4}${5}\r${6}`"), [4,5,6]);

// Chained calls
function func(a) {
    if (a[0] === "hey") {
        return function(a) {
            if (a[0] === "there") {
                return function(a) {
                    if (a[0] === "mine")
                        return "was mine";
                    else
                        return "was not mine";
                }
            } else {
                return function(a) {
                    return "was not there";
                }
            }
        }
    } else {
        return function(a) {
            return function(a) {
                return "was not hey";
            }
        }
    }
}

assertEq(func`hey``there``mine`, "was mine");
assertEq(func`hey``there``amine`, "was not mine");
assertEq(func`hey``tshere``amine`, "was not there");
assertEq(func`heys``there``mine`, "was not hey");

// String.raw
assertEq(String.raw`h\r\ney${4}there\n`, "h\\r\\ney4there\\n");
assertEq(String.raw`hey`, "hey");
assertEq(String.raw``, "");


reportCompare(0, 0, "ok");
