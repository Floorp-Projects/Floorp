/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Destructuring binding patterns with var.
(function() {
    var {a: yi\u0065ld} = {a: "yield-with-name"};
    assertEq(yield, "yield-with-name");

    var {yi\u0065ld} = {yield: "yield-with-shorthand"};
    assertEq(yield, "yield-with-shorthand");

    var {yi\u0065ld = 0} = {yield: "yield-with-coverinitname"};
    assertEq(yield, "yield-with-coverinitname");
})();

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f() {
        var {a: yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f() {
        var {yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f() {
        var {yi\u0065ld = 0} = {};
    }
`), SyntaxError);


// Destructuring binding patterns with let.
(function(){
    let {a: yi\u0065ld} = {a: "yield-with-name"};
    assertEq(yield, "yield-with-name");
})();

(function() {
    let {yi\u0065ld} = {yield: "yield-with-shorthand"};
    assertEq(yield, "yield-with-shorthand");
})();

(function() {
    let {yi\u0065ld = 0} = {yield: "yield-with-coverinitname"};
    assertEq(yield, "yield-with-coverinitname");
})();

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f() {
        let {a: yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f() {
        let {yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f() {
        let {yi\u0065ld = 0} = {};
    }
`), SyntaxError);


// Destructuring binding patterns with const.
(function() {
    const {a: yi\u0065ld} = {a: "yield-with-name"};
    assertEq(yield, "yield-with-name");
})();

(function() {
    const {yi\u0065ld} = {yield: "yield-with-shorthand"};
    assertEq(yield, "yield-with-shorthand");
})();

(function() {
    const {yi\u0065ld = 0} = {yield: "yield-with-coverinitname"};
    assertEq(yield, "yield-with-coverinitname");
})();

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f() {
        const {a: yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f() {
        const {yi\u0065ld} = {};
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f() {
        const {yi\u0065ld = 0} = {};
    }
`), SyntaxError);


// Destructuring binding patterns in parameters.
(function({a: yi\u0065ld} = {a: "yield-with-name"}) {
    assertEq(yield, "yield-with-name");
})();

(function({yi\u0065ld} = {yield: "yield-with-shorthand"}) {
    assertEq(yield, "yield-with-shorthand");
})();

(function({yi\u0065ld = 0} = {yield: "yield-with-coverinitname"}) {
    assertEq(yield, "yield-with-coverinitname");
})();

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f({a: yi\u0065ld} = {}) { }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f({yi\u0065ld} = {}) { }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f({yi\u0065ld = 0} = {}) { }
`), SyntaxError);


// Destructuring assignment pattern.
(function() {
    var a, yield;

    ({a: yi\u0065ld} = {a: "yield-with-name"});
    assertEq(yield, "yield-with-name");

    ({yi\u0065ld} = {yield: "yield-with-shorthand"});
    assertEq(yield, "yield-with-shorthand");

    ({yi\u0065ld = 0} = {yield: "yield-with-coverinitname"});
    assertEq(yield, "yield-with-coverinitname");
})();

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f() {
        ({a: yi\u0065ld} = {});
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f() {
        ({yi\u0065ld} = {});
    }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    function f() {
        ({yi\u0065ld = 0} = {});
    }
`), SyntaxError);


if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
