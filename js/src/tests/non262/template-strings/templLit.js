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

// TEST BEGIN



// combinations of substitutions
assertEq("abcdef", `ab${"cd"}ef`);
assertEq("ab9ef", `ab${4+5}ef`);
assertEq("cdef", `${"cd"}ef`);
assertEq("abcd", `ab${"cd"}`);
assertEq("cd", `${"cd"}`);
assertEq("", `${""}`);
assertEq("4", `${4}`);

// multiple substitutions
assertEq("abcdef", `ab${"cd"}e${"f"}`);
assertEq("abcdef", `ab${"cd"}${"e"}f`);
assertEq("abcdef", `a${"b"}${"cd"}e${"f"}`);
assertEq("abcdef", `${"ab"}${"cd"}${"ef"}`);

// inception
assertEq("abcdef", `a${`b${"cd"}e${"f"}`}`);

syntaxError("`${}`");
syntaxError("`${`");
syntaxError("`${\\n}`");
syntaxError("`${yield 0}`");

// Extra whitespace inside a template substitution is ignored.
assertEq(`${
0
}`, "0");

assertEq(`${ // Comments work in template substitutions.
// Even comments that look like code:
// 0}`, "FAIL");   /* NOTE: This whole line is a comment.
0}`, "0");

// Template substitutions are expressions, not statements.
syntaxError("`${0;}`");
assertEq(`${{}}`, "[object Object]");
assertEq(`${
    function f() {
        return "ok";
    }()
}`, "ok");

// Template substitutions can have side effects.
var x = 0;
assertEq(`= ${x += 1}`, "= 1");
assertEq(x, 1);

// The production for a template substitution is Expression, not
// AssignmentExpression.
x = 0;
assertEq(`${++x, "o"}k`, "ok");
assertEq(x, 1);

// --> is not a comment inside a template.
assertEq(`
--> this is text
`, "\n--> this is text\n");

// reentrancy
function f(n) {
    if (n === 0)
        return "";
    return `${n}${f(n - 1)}`;
}
assertEq(f(9), "987654321");

// Template string substitutions in generator functions can yield.
function* g() {
    return `${yield 1} ${yield 2}`;
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
assertEq(next.value, "hello world");

// undefined
assertEq(`${void 0}`, "undefined");
assertEq(`${Object.doesNotHaveThisProperty}`, "undefined");

// toString behavior
assertEq("<toString>", `${{valueOf: () => "<valueOf>", toString: () => "<toString>"}}`);
assertEq("Hi 42", Function("try {`${{toString: () => { throw 42;}}}`} catch(e) {return \"Hi \" + e;}")());


reportCompare(0, 0, "ok");
