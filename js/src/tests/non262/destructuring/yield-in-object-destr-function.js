/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Destructuring binding patterns with var.
(function() {
    var {a: yield} = {a: "yield-with-name"};
    assertEq(yield, "yield-with-name");

    var {yield} = {yield: "yield-with-shorthand"};
    assertEq(yield, "yield-with-shorthand");

    var {yield = 0} = {yield: "yield-with-coverinitname"};
    assertEq(yield, "yield-with-coverinitname");
})();

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f() {
        var {a: yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f() {
        var {yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f() {
        var {yield = 0} = {};
    }
`), SyntaxError);


// Destructuring binding patterns with let.
(function(){
    let {a: yield} = {a: "yield-with-name"};
    assertEq(yield, "yield-with-name");
})();

(function() {
    let {yield} = {yield: "yield-with-shorthand"};
    assertEq(yield, "yield-with-shorthand");
})();

(function() {
    let {yield = 0} = {yield: "yield-with-coverinitname"};
    assertEq(yield, "yield-with-coverinitname");
})();

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f() {
        let {a: yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f() {
        let {yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f() {
        let {yield = 0} = {};
    }
`), SyntaxError);


// Destructuring binding patterns with const.
(function() {
    const {a: yield} = {a: "yield-with-name"};
    assertEq(yield, "yield-with-name");
})();

(function() {
    const {yield} = {yield: "yield-with-shorthand"};
    assertEq(yield, "yield-with-shorthand");
})();

(function() {
    const {yield = 0} = {yield: "yield-with-coverinitname"};
    assertEq(yield, "yield-with-coverinitname");
})();

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f() {
        const {a: yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f() {
        const {yield} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f() {
        const {yield = 0} = {};
    }
`), SyntaxError);


// Destructuring binding patterns in parameters.
(function({a: yield} = {a: "yield-with-name"}) {
    assertEq(yield, "yield-with-name");
})();

(function({yield} = {yield: "yield-with-shorthand"}) {
    assertEq(yield, "yield-with-shorthand");
})();

(function({yield = 0} = {yield: "yield-with-coverinitname"}) {
    assertEq(yield, "yield-with-coverinitname");
})();

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f({a: yield} = {}) { }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f({yield} = {}) { }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f({yield = 0} = {}) { }
`), SyntaxError);


// Destructuring assignment pattern.
(function() {
    var a, yield;

    ({a: yield} = {a: "yield-with-name"});
    assertEq(yield, "yield-with-name");

    ({yield} = {yield: "yield-with-shorthand"});
    assertEq(yield, "yield-with-shorthand");

    ({yield = 0} = {yield: "yield-with-coverinitname"});
    assertEq(yield, "yield-with-coverinitname");
})();

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f() {
        ({a: yield} = {});
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f() {
        ({yield} = {});
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    function f() {
        ({yield = 0} = {});
    }
`), SyntaxError);


if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
