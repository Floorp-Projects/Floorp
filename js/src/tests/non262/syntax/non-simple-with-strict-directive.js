/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const testCases = [
    // Array destructuring.
    "[]",
    "[a]",
    "x, [a]",
    "[a], x",

    // Array destructuring with defaults.
    "[a = 0]",
    "x, [a = 0]",
    "[a = 0], x",

    // Array destructuring with rest binding identifier.
    "[...a]",
    "x, [...a]",
    "[...a], x",

    // Array destructuring with rest binding pattern.
    "[...[a]]",
    "x, [...[a]]",
    "[...[a]], x",

    // Object destructuring.
    "{}",
    "{p: o}",
    "x, {p: o}",
    "{p: o}, x",

    // Object destructuring with defaults.
    "{p: o = 0}",
    "x, {p: o = 0}",
    "{p: o = 0}, x",

    // Object destructuring with shorthand identifier form.
    "{o}",
    "x, {o}",
    "{o}, x",

    // Object destructuring with CoverInitName.
    "{o = 0}",
    "x, {o = 0}",
    "{o = 0}, x",

    // Default parameter.
    "d = 0",
    "x, d = 0",
    "d = 0, x",

    // Rest parameter.
    "...rest",
    "x, ...rest",

    // Rest parameter with array destructuring.
    "...[]",
    "...[a]",
    "x, ...[]",
    "x, ...[a]",

    // Rest parameter with object destructuring.
    "...{}",
    "...{p: o}",
    "x, ...{}",
    "x, ...{p: o}",

    // All non-simple cases combined.
    "x, d = 123, [a], {p: 0}, ...rest",
];

const GeneratorFunction = function*(){}.constructor;

const functionDefinitions = [
    // FunctionDeclaration
    parameters => `function f(${parameters}) { "use strict"; }`,

    // FunctionExpression
    parameters => `void function(${parameters}) { "use strict"; };`,
    parameters => `void function f(${parameters}) { "use strict"; };`,

    // Function constructor
    parameters => `Function('${parameters}', '"use strict";')`,

    // GeneratorDeclaration
    parameters => `function* g(${parameters}) { "use strict"; }`,

    // GeneratorExpression
    parameters => `void function*(${parameters}) { "use strict"; };`,
    parameters => `void function* g(${parameters}) { "use strict"; };`,

    // GeneratorFunction constructor
    parameters => `GeneratorFunction('${parameters}', '"use strict";')`,

    // MethodDefinition
    parameters => `({ m(${parameters}) { "use strict"; } });`,
    parameters => `(class { m(${parameters}) { "use strict"; } });`,
    parameters => `class C { m(${parameters}) { "use strict"; } }`,

    // MethodDefinition (constructors)
    parameters => `(class { constructor(${parameters}) { "use strict"; } });`,
    parameters => `class C { constructor(${parameters}) { "use strict"; } }`,

    // MethodDefinition (getter)
    parameters => `({ get m(${parameters}) { "use strict"; } });`,
    parameters => `(class { get m(${parameters}) { "use strict"; } });`,
    parameters => `class C { get m(${parameters}) { "use strict"; } }`,

    // MethodDefinition (setter)
    parameters => `({ set m(${parameters}) { "use strict"; } });`,
    parameters => `(class { set m(${parameters}) { "use strict"; } });`,
    parameters => `class C { set m(${parameters}) { "use strict"; } }`,

    // GeneratorMethod
    parameters => `({ *m(${parameters}) { "use strict"; } });`,
    parameters => `(class { *m(${parameters}) { "use strict"; } });`,
    parameters => `class C { *m(${parameters}) { "use strict"; } }`,

    // ArrowFunction
    parameters => `(${parameters}) => { "use strict"; };`,
];

for (let nonSimpleParameters of testCases) {
    for (let def of functionDefinitions) {
        // Non-strict script code.
        assertThrowsInstanceOf(() => eval(`
            ${def(nonSimpleParameters)}
        `), SyntaxError, def(nonSimpleParameters));

        // Strict script code.
        assertThrowsInstanceOf(() => eval(`
            "use strict";
            ${def(nonSimpleParameters)}
        `), SyntaxError, `"use strict"; ${def(nonSimpleParameters)}`);
    }
}

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
