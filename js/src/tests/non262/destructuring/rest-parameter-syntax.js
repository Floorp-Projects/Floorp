/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const bindingPatterns = [
    "[]",
    "[a]",
    "[a, b]",
    "[a, ...b]",
    "[...a]",
    "[...[]]",

    "{}",
    "{p: a}",
    "{p: a = 0}",
    "{p: {}}",
    "{p: a, q: b}",
    "{a}",
    "{a, b}",
    "{a = 0}",
];

const functions = [
    p => `function f(${p}) {}`,
    p => `function* g(${p}) {}`,
    p => `({m(${p}) {}});`,
    p => `(class {m(${p}) {}});`,
    p => `(${p}) => {};`,
];

for (let pattern of bindingPatterns) {
    for (let fn of functions) {
        // No leading parameters.
        eval(fn(`...${pattern}`));

        // Leading normal parameters.
        eval(fn(`x, ...${pattern}`));
        eval(fn(`x, y, ...${pattern}`));

        // Leading parameters with defaults.
        eval(fn(`x = 0, ...${pattern}`));
        eval(fn(`x = 0, y = 0, ...${pattern}`));

        // Leading array destructuring parameters.
        eval(fn(`[], ...${pattern}`));
        eval(fn(`[x], ...${pattern}`));
        eval(fn(`[x = 0], ...${pattern}`));
        eval(fn(`[...x], ...${pattern}`));

        // Leading object destructuring parameters.
        eval(fn(`{}, ...${pattern}`));
        eval(fn(`{p: x}, ...${pattern}`));
        eval(fn(`{x}, ...${pattern}`));
        eval(fn(`{x = 0}, ...${pattern}`));

        // Trailing parameters after rest parameter.
        assertThrowsInstanceOf(() => eval(fn(`...${pattern},`)), SyntaxError);
        assertThrowsInstanceOf(() => eval(fn(`...${pattern}, x`)), SyntaxError);
        assertThrowsInstanceOf(() => eval(fn(`...${pattern}, x = 0`)), SyntaxError);
        assertThrowsInstanceOf(() => eval(fn(`...${pattern}, ...x`)), SyntaxError);
        assertThrowsInstanceOf(() => eval(fn(`...${pattern}, []`)), SyntaxError);
        assertThrowsInstanceOf(() => eval(fn(`...${pattern}, {}`)), SyntaxError);

        // Rest parameter with defaults.
        assertThrowsInstanceOf(() => eval(fn(`...${pattern} = 0`)), SyntaxError);
    }
}

for (let fn of functions) {
    // Missing name, incomplete patterns.
    assertThrowsInstanceOf(() => eval(fn(`...`)), SyntaxError);
    assertThrowsInstanceOf(() => eval(fn(`...[`)), SyntaxError);
    assertThrowsInstanceOf(() => eval(fn(`...{`)), SyntaxError);

    // Invalid binding name.
    assertThrowsInstanceOf(() => eval(fn(`...[0]`)), SyntaxError);
    assertThrowsInstanceOf(() => eval(fn(`...[p.q]`)), SyntaxError);
}

// Rest parameters aren't valid in getter/setter methods.
assertThrowsInstanceOf(() => eval(`({get p(...[]) {}})`), SyntaxError);
assertThrowsInstanceOf(() => eval(`({set p(...[]) {}})`), SyntaxError);


if (typeof reportCompare === "function")
    reportCompare(0, 0);
