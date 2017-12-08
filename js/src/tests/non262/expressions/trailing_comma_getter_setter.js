/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Trailing comma is not allowed in getter and setter methods

// 14.3 Method Definitions
// MethodDefinition[Yield]:
//   get PropertyName[?Yield] () { FunctionBody[~Yield] }
//   set PropertyName[?Yield] ( PropertySetParameterList ) { FunctionBody[~Yield] }
// PropertySetParameterList:
//   FormalParameter[~Yield]

function objectGetter(argList) {
    return eval(`({
        get m(${argList}) {}
    })`);
}

function objectSetter(argList) {
    return eval(`({
        set m(${argList}) {}
    })`);
}

function classGetter(argList) {
    return eval(`(class {
        get m(${argList}) {}
    })`);
}

function classStaticGetter(argList) {
    return eval(`(class {
        static get m(${argList}) {}
    })`);
}

function classSetter(argList) {
    return eval(`(class {
        set m(${argList}) {}
    })`);
}

function classStaticSetter(argList) {
    return eval(`(class {
        static set m(${argList}) {}
    })`);
}

const tests = [
    objectGetter,
    objectSetter,
    classGetter,
    classStaticGetter,
    classSetter,
    classStaticSetter,
];

for (let test of tests) {
    // Trailing comma.
    assertThrowsInstanceOf(() => test("a, "), SyntaxError);
    assertThrowsInstanceOf(() => test("[], "), SyntaxError);
    assertThrowsInstanceOf(() => test("{}, "), SyntaxError);
    assertThrowsInstanceOf(() => test("a = 0, "), SyntaxError);
    assertThrowsInstanceOf(() => test("[] = [], "), SyntaxError);
    assertThrowsInstanceOf(() => test("{} = {}, "), SyntaxError);

    // Trailing comma in empty parameters list.
    assertThrowsInstanceOf(() => test(","), SyntaxError);

    // Leading comma.
    assertThrowsInstanceOf(() => test(", a"), SyntaxError);
    assertThrowsInstanceOf(() => test(", ...a"), SyntaxError);

    // Multiple trailing comma.
    assertThrowsInstanceOf(() => test("a, ,"), SyntaxError);
    assertThrowsInstanceOf(() => test("a..., ,"), SyntaxError);

    // Trailing comma after rest parameter.
    assertThrowsInstanceOf(() => test("...a ,"), SyntaxError);
    assertThrowsInstanceOf(() => test("a, ...b, "), SyntaxError);

    // Elision.
    assertThrowsInstanceOf(() => test("a, , b"), SyntaxError);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
