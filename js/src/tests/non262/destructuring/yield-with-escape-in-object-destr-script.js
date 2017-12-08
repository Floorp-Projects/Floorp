/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Destructuring binding patterns with var.
var {a: yi\u0065ld} = {a: "yield-with-name"};
assertEq(yield, "yield-with-name");

var {yi\u0065ld} = {yield: "yield-with-shorthand"};
assertEq(yield, "yield-with-shorthand");

var {yi\u0065ld = 0} = {yield: "yield-with-coverinitname"};
assertEq(yield, "yield-with-coverinitname");

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    var {a: yi\u0065ld} = {};
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    var {yi\u0065ld} = {};
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    var {yi\u0065ld = 0} = {};
`), SyntaxError);


// Destructuring binding patterns with let.
{
    let {a: yi\u0065ld} = {a: "yield-with-name"};
    assertEq(yield, "yield-with-name");
}

{
    let {yi\u0065ld} = {yield: "yield-with-shorthand"};
    assertEq(yield, "yield-with-shorthand");
}

{
    let {yi\u0065ld = 0} = {yield: "yield-with-coverinitname"};
    assertEq(yield, "yield-with-coverinitname");
}

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    let {a: yi\u0065ld} = {};
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    let {yi\u0065ld} = {};
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    let {yi\u0065ld = 0} = {};
`), SyntaxError);


// Destructuring binding patterns with const.
{
    const {a: yi\u0065ld} = {a: "yield-with-name"};
    assertEq(yield, "yield-with-name");
}

{
    const {yi\u0065ld} = {yield: "yield-with-shorthand"};
    assertEq(yield, "yield-with-shorthand");
}

{
    const {yi\u0065ld = 0} = {yield: "yield-with-coverinitname"};
    assertEq(yield, "yield-with-coverinitname");
}

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    const {a: yi\u0065ld} = {};
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    const {yi\u0065ld} = {};
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    const {yi\u0065ld = 0} = {};
`), SyntaxError);


// Destructuring assignment pattern.
({a: yi\u0065ld} = {a: "yield-with-name"});
assertEq(yield, "yield-with-name");

({yi\u0065ld} = {yield: "yield-with-shorthand"});
assertEq(yield, "yield-with-shorthand");

({yi\u0065ld = 0} = {yield: "yield-with-coverinitname"});
assertEq(yield, "yield-with-coverinitname");

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    ({a: yi\u0065ld} = {});
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    ({yi\u0065ld} = {});
`), SyntaxError);

assertThrowsInstanceOf(() => eval(String.raw`
    "use strict";
    ({yi\u0065ld = 0} = {});
`), SyntaxError);


if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
