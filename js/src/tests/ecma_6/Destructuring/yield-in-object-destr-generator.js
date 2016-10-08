/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Destructuring binding patterns with var.
assertThrowsInstanceOf(() => eval(`
    function* g() {
        var {a: yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    function* g() {
        var {yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    function* g() {
        var {yield = 0} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g() {
        var {a: yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g() {
        var {yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g() {
        var {yield = 0} = {};
    }
`), SyntaxError);


// Destructuring binding patterns with let.
assertThrowsInstanceOf(() => eval(`
    function* g() {
        let {a: yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    function* g() {
        let {yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    function* g() {
        let {yield = 0} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g() {
        let {a: yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g() {
        let {yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g() {
        let {yield = 0} = {};
    }
`), SyntaxError);


// Destructuring binding patterns with const.
assertThrowsInstanceOf(() => eval(`
    function* g() {
        const {a: yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    function* g() {
        const {yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    function* g() {
        const {yield = 0} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g() {
        const {a: yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g() {
        const {yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g() {
        const {yield = 0} = {};
    }
`), SyntaxError);


// Destructuring binding patterns in parameters.
assertThrowsInstanceOf(() => eval(`
    function* g({a: yield} = {}) { }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    function* g({yield} = {}) { }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    function* g({yield = 0} = {}) { }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g({a: yield} = {}) { }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g({yield} = {}) { }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g({yield = 0} = {}) { }
`), SyntaxError);


// Destructuring assignment pattern.
assertThrowsInstanceOf(() => eval(`
    function* g() {
        ({a: yield} = {});
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    function* g() {
        ({yield} = {});
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    function* g() {
        ({yield = 0} = {});
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g() {
        ({a: yield} = {});
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g() {
        ({yield} = {});
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function* g() {
        ({yield = 0} = {});
    }
`), SyntaxError);


if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
