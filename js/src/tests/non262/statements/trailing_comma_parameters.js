/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Trailing comma in functions and generators.

// 14.1 Function Definitions
// FunctionDeclaration[Yield, Default]:
//   function BindingIdentifier[?Yield] ( FormalParameters[~Yield] ) { FunctionBody[~Yield] }

// 14.4 Generator Function Definitions
// GeneratorDeclaration[Yield, Default]:
//   function * BindingIdentifier[?Yield] ( FormalParameters[+Yield] ) { GeneratorBody }


function functionDeclaration(argList, parameters = "", returnExpr = "") {
    return eval(`
        function f(${argList}) {
            var fun = f;
            return ${returnExpr};
        }
        f(${parameters});
    `);
}

function generatorDeclaration(argList, parameters = "", returnExpr = "") {
    return eval(`
        function* f(${argList}) {
            var fun = f;
            return ${returnExpr};
        }
        f(${parameters}).next().value;
    `);
}

const tests = [
    functionDeclaration,
    generatorDeclaration,
];

// Ensure parameters are passed correctly.
for (let test of tests) {
    assertEq(test("a, ", "10", "a"), 10);
    assertEq(test("a, b, ", "10, 20", "a + b"), 30);
    assertEq(test("a = 30, ", "", "a"), 30);
    assertEq(test("a = 30, b = 40, ", "", "a + b"), 70);

    assertEq(test("[a], ", "[10]", "a"), 10);
    assertEq(test("[a], [b], ", "[10], [20]", "a + b"), 30);
    assertEq(test("[a] = [30], ", "", "a"), 30);
    assertEq(test("[a] = [30], [b] = [40], ", "", "a + b"), 70);

    assertEq(test("{a}, ", "{a: 10}", "a"), 10);
    assertEq(test("{a}, {b}, ", "{a: 10}, {b: 20}", "a + b"), 30);
    assertEq(test("{a} = {a: 30}, ", "", "a"), 30);
    assertEq(test("{a} = {a: 30}, {b} = {b: 40}, ", "", "a + b"), 70);
}

// Ensure function length doesn't change.
for (let test of tests) {
    assertEq(test("a, ", "", "fun.length"), 1);
    assertEq(test("a, b, ", "", "fun.length"), 2);

    assertEq(test("[a], ", "[]", "fun.length"), 1);
    assertEq(test("[a], [b], ", "[], []", "fun.length"), 2);

    assertEq(test("{a}, ", "{}", "fun.length"), 1);
    assertEq(test("{a}, {b}, ", "{}, {}", "fun.length"), 2);
}

for (let test of tests) {
    // Trailing comma in empty parameters list.
    assertThrowsInstanceOf(() => test(","), SyntaxError);

    // Leading comma.
    assertThrowsInstanceOf(() => test(", a"), SyntaxError);
    assertThrowsInstanceOf(() => test(", ...a"), SyntaxError);

    // Multiple trailing comma.
    assertThrowsInstanceOf(() => test("a, , "), SyntaxError);
    assertThrowsInstanceOf(() => test("a..., , "), SyntaxError);

    // Trailing comma after rest parameter.
    assertThrowsInstanceOf(() => test("...a, "), SyntaxError);
    assertThrowsInstanceOf(() => test("a, ...b, "), SyntaxError);

    // Elision.
    assertThrowsInstanceOf(() => test("a, , b"), SyntaxError);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
