/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Trailing comma in functions and methods.

// 14.1 Function Definitions
// FunctionExpression:
//   function BindingIdentifier[~Yield]opt ( FormalParameters[~Yield] ) { FunctionBody[~Yield] }

// 14.3 Method Definitions
// MethodDefinition[Yield]:
//   PropertyName[?Yield] ( UniqueFormalParameters[~Yield] ) { FunctionBody[~Yield] }
//   GeneratorMethod[?Yield]
// PropertySetParameterList:
//   FormalParameter[~Yield]

// 14.4 Generator Function Definitions
// GeneratorExpression:
//   function * BindingIdentifier[+Yield]opt ( FormalParameters[+Yield] ) { GeneratorBody }
// GeneratorMethod[Yield]:
//   * PropertyName[?Yield] ( UniqueFormalParameters[+Yield] ) { GeneratorBody }


function functionExpression(argList, parameters = "", returnExpr = "") {
    return eval(`(function f(${argList}) {
        var fun = f;
        return ${returnExpr};
    })(${parameters})`);
}

function generatorExpression(argList, parameters = "", returnExpr = "") {
    return eval(`(function* f(${argList}) {
        var fun = f;
        return ${returnExpr};
    })(${parameters}).next().value`);
}

function objectMethod(argList, parameters = "", returnExpr = "") {
    return eval(`({
        m(${argList}) {
            var fun = this.m;
            return ${returnExpr};
        }
    }).m(${parameters})`);
}

function objectGeneratorMethod(argList, parameters = "", returnExpr = "") {
    return eval(`({
        * m(${argList}) {
            var fun = this.m;
            return ${returnExpr};
        }
    }).m(${parameters}).next().value`);
}

function classMethod(argList, parameters = "", returnExpr = "") {
    return eval(`(new class {
        m(${argList}) {
            var fun = this.m;
            return ${returnExpr};
        }
    }).m(${parameters})`);
}

function classStaticMethod(argList, parameters = "", returnExpr = "") {
    return eval(`(class {
        static m(${argList}) {
            var fun = this.m;
            return ${returnExpr};
        }
    }).m(${parameters})`);
}

function classGeneratorMethod(argList, parameters = "", returnExpr = "") {
    return eval(`(new class {
        * m(${argList}) {
            var fun = this.m;
            return ${returnExpr};
        }
    }).m(${parameters}).next().value`);
}

function classStaticGeneratorMethod(argList, parameters = "", returnExpr = "") {
    return eval(`(class {
        static * m(${argList}) {
            var fun = this.m;
            return ${returnExpr};
        }
    }).m(${parameters}).next().value`);
}

function classConstructorMethod(argList, parameters = "", returnExpr = "null") {
    return eval(`new (class {
        constructor(${argList}) {
            var fun = this.constructor;
            return { value: ${returnExpr} };
        }
    })(${parameters}).value`);
}

const tests = [
    functionExpression,
    generatorExpression,
    objectMethod,
    objectGeneratorMethod,
    classMethod,
    classStaticMethod,
    classGeneratorMethod,
    classStaticGeneratorMethod,
    classConstructorMethod,
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
