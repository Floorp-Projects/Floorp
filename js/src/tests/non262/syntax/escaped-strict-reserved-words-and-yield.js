/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const testCases = [
    // Label identifier.
    id => `${id}: ;`,

    // Binding identifier.
    id => `var ${id};`,
    id => `let ${id};`,
    id => `const ${id} = 0;`,

    // Binding identifier in binding pattern.
    id => `var [${id}] = [];`,
    id => `var [${id} = 0] = [];`,
    id => `var [...${id}] = [];`,
    id => `var {a: ${id}} = {};`,
    id => `var {${id}} = {};`,
    id => `var {${id} = 0} = {};`,

    id => `let [${id}] = [];`,
    id => `let [${id} = 0] = [];`,
    id => `let [...${id}] = [];`,
    id => `let {a: ${id}} = {};`,
    id => `let {${id}} = {};`,
    id => `let {${id} = 0} = {};`,

    id => `const [${id}] = [];`,
    id => `const [${id} = 0] = [];`,
    id => `const [...${id}] = [];`,
    id => `const {a: ${id}} = {};`,
    id => `const {${id}} = {};`,
    id => `const {${id} = 0} = {};`,

    // Identifier reference.
    id => `void ${id};`,
];

const strictReservedWords = [
    "implements",
    "interface",
    "package",
    "private",
    "protected",
    "public",
];

function escapeWord(s) {
    return "\\u00" + s.charCodeAt(0).toString(16) + s.substring(1);
}

for (let strictReservedWordOrYield of [...strictReservedWords, "yield"]) {
    let escapedStrictReservedWordOrYield = escapeWord(strictReservedWordOrYield);

    for (let testCase of testCases) {
        eval(testCase(strictReservedWordOrYield));
        eval(testCase(escapedStrictReservedWordOrYield));

        assertThrowsInstanceOf(() => eval(String.raw`
            "use strict";
            ${testCase(strictReservedWordOrYield)}
        `), SyntaxError);

        assertThrowsInstanceOf(() => eval(String.raw`
            "use strict";
            ${testCase(escapedStrictReservedWordOrYield)}
        `), SyntaxError);
    }
}

// |yield| is always a keyword in generator functions.
for (let testCase of testCases) {
    let yield = "yield";
    let escapedYield = escapeWord("yield");

    assertThrowsInstanceOf(() => eval(String.raw`
        function* g() {
            ${testCase(yield)}
        }
    `), SyntaxError);

    assertThrowsInstanceOf(() => eval(String.raw`
        function* g() {
            ${testCase(escapedYield)}
        }
    `), SyntaxError);

    assertThrowsInstanceOf(() => eval(String.raw`
        "use strict";
        function* g() {
            ${testCase(yield)}
        }
    `), SyntaxError);

    assertThrowsInstanceOf(() => eval(String.raw`
        "use strict";
        function* g() {
            ${testCase(escapedYield)}
        }
    `), SyntaxError);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
