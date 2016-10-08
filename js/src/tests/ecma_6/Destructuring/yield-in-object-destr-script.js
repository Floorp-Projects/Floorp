/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


// Destructuring binding patterns with var.
var {a: yield} = {a: "yield-with-name"};
assertEq(yield, "yield-with-name");

var {yield} = {yield: "yield-with-shorthand"};
assertEq(yield, "yield-with-shorthand");

var {yield = 0} = {yield: "yield-with-coverinitname"};
assertEq(yield, "yield-with-coverinitname");

assertThrowsInstanceOf(() => eval(`
    "use strict";
    var {a: yield} = {};
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    var {yield} = {};
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    var {yield = 0} = {};
`), SyntaxError);


// Destructuring binding patterns with let.
{
    let {a: yield} = {a: "yield-with-name"};
    assertEq(yield, "yield-with-name");
}

{
    let {yield} = {yield: "yield-with-shorthand"};
    assertEq(yield, "yield-with-shorthand");
}

{
    let {yield = 0} = {yield: "yield-with-coverinitname"};
    assertEq(yield, "yield-with-coverinitname");
}

assertThrowsInstanceOf(() => eval(`
    "use strict";
    let {a: yield} = {};
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    let {yield} = {};
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    let {yield = 0} = {};
`), SyntaxError);


// Destructuring binding patterns with const.
{
    const {a: yield} = {a: "yield-with-name"};
    assertEq(yield, "yield-with-name");
}

{
    const {yield} = {yield: "yield-with-shorthand"};
    assertEq(yield, "yield-with-shorthand");
}

{
    const {yield = 0} = {yield: "yield-with-coverinitname"};
    assertEq(yield, "yield-with-coverinitname");
}

assertThrowsInstanceOf(() => eval(`
    "use strict";
    const {a: yield} = {};
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    const {yield} = {};
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    const {yield = 0} = {};
`), SyntaxError);


// Destructuring assignment pattern.
({a: yield} = {a: "yield-with-name"});
assertEq(yield, "yield-with-name");

({yield} = {yield: "yield-with-shorthand"});
assertEq(yield, "yield-with-shorthand");

({yield = 0} = {yield: "yield-with-coverinitname"});
assertEq(yield, "yield-with-coverinitname");

assertThrowsInstanceOf(() => eval(`
    "use strict";
    ({a: yield} = {});
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    ({yield} = {});
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
    "use strict";
    ({yield = 0} = {});
`), SyntaxError);


if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
