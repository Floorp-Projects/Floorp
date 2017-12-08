/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Destructuring binding patterns with var.
assertThrowsInstanceOf(() => eval(String.raw`
    function* g() {
        var {a: yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    function* g() {
        var {yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    function* g() {
        var {yi\u0065ld = 0} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g() {
        var {a: yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g() {
        var {yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g() {
        var {yi\u0065ld = 0} = {};
    }
`), SyntaxError);


// Destructuring binding patterns with let.
assertThrowsInstanceOf(() => eval(String.raw`
    function* g() {
        let {a: yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    function* g() {
        let {yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    function* g() {
        let {yi\u0065ld = 0} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g() {
        let {a: yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g() {
        let {yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g() {
        let {yi\u0065ld = 0} = {};
    }
`), SyntaxError);


// Destructuring binding patterns with const.
assertThrowsInstanceOf(() => eval(String.raw`
    function* g() {
        const {a: yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    function* g() {
        const {yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    function* g() {
        const {yi\u0065ld = 0} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g() {
        const {a: yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g() {
        const {yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g() {
        const {yi\u0065ld = 0} = {};
    }
`), SyntaxError);


// Destructuring binding patterns in parameters.
assertThrowsInstanceOf(() => eval(String.raw`
    function* g({a: yi\u0065ld} = {}) { }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    function* g({yi\u0065ld} = {}) { }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    function* g({yi\u0065ld = 0} = {}) { }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g({a: yi\u0065ld} = {}) { }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g({yi\u0065ld} = {}) { }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g({yi\u0065ld = 0} = {}) { }
`), SyntaxError);


// Destructuring assignment pattern.
assertThrowsInstanceOf(() => eval(String.raw`
    function* g() {
        ({a: yi\u0065ld} = {});
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    function* g() {
        ({yi\u0065ld} = {});
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    function* g() {
        ({yi\u0065ld = 0} = {});
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g() {
        ({a: yi\u0065ld} = {});
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g() {
        ({yi\u0065ld} = {});
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function* g() {
        ({yi\u0065ld = 0} = {});
    }
`), SyntaxError);


if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
