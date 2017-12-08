/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Trailing comma in CoverParenthesizedExpressionAndArrowParameterList production.

// 12.2 Primary Expression
// CoverParenthesizedExpressionAndArrowParameterList[Yield]:
//   ( Expression[In, ?Yield] )
//   ( Expression[In, ?Yield] , )
//   ()
//   ( ...BindingIdentifier[?Yield] )
//   ( Expression[In, ?Yield] , ...BindingIdentifier[?Yield] )


function arrow(argList, parameters = "", returnExpr = "") {
    return eval(`
        let fun = (${argList}) => {
            return ${returnExpr};
        }
        fun(${parameters});
    `);
}

function arrowConcise(argList, parameters = "", returnExpr = "null") {
    return eval(`
        let fun = (${argList}) => ${returnExpr};
        fun(${parameters});
    `);
}

const tests = [
    arrow,
    arrowConcise,
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

// Trailing comma in non-parenthesized arrow head.
assertThrowsInstanceOf(() => eval("a, => {}"), SyntaxError);
assertThrowsInstanceOf(() => eval("a, => null"), SyntaxError);

// Parenthesized expression is not an arrow function expression.
for (let trail of ["", ";", "\n => {}"]) {
    assertThrowsInstanceOf(() => eval("(a,)" + trail), SyntaxError);
    assertThrowsInstanceOf(() => eval("(a, b,)" + trail), SyntaxError);
    assertThrowsInstanceOf(() => eval("(...a, )" + trail), SyntaxError);
    assertThrowsInstanceOf(() => eval("(a, ...b, )" + trail), SyntaxError);
    assertThrowsInstanceOf(() => eval("(a, , b)" + trail), SyntaxError);

    assertThrowsInstanceOf(() => eval("(,)" + trail), SyntaxError);
    assertThrowsInstanceOf(() => eval("(, a)" + trail), SyntaxError);
    assertThrowsInstanceOf(() => eval("(, ...a)" + trail), SyntaxError);
    assertThrowsInstanceOf(() => eval("(a, , )" + trail), SyntaxError);
    assertThrowsInstanceOf(() => eval("(...a, , )" + trail), SyntaxError);
}


if (typeof reportCompare === "function")
    reportCompare(0, 0);
