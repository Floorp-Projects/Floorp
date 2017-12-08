/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Ensure that the syntax used in shorthand destructuring with defaults
// e.g. |{x=1, y=2} = {}| properly raises a syntax error within an object
// literal. As per ES6 12.2.6 Object Initializer, "NOTE 3."

const SYNTAX_ERROR_STMTS = [
    // expressions
    "({x={}={}}),",
    "({y={x={}={}={}={}={}={}={}={}}={}}),",
    "({a=1, b=2, c=3, x=({}={})}),",
    "({x=1, y={z={1}}})",
    "({x=1} = {y=1});",
    "({x: y={z=1}}={})",
    "({x=1}),",
    "({z={x=1}})=>{};",
    "({x = ({y=1}) => y})",
    "(({x=1})) => x",
    "({e=[]}==(;",
    "({x=1}[-1]);",
    "({x=y}[-9])",
    "({x=y}.x.z[-9])",
    "({x=y}`${-9}`)",
    "(new {x=y}(-9))",
    "new {x=1}",
    "new {x=1}={}",
    "typeof {x=1}",
    "typeof ({x=1})",
    "({x=y, [-9]:0})",
    "((({w = x} >(-9)",
    "++({x=1})",
    "--{x=1}",
    "!{x=1}={}",
    "delete {x=1}",
    "delete ({x=1})",
    "delete {x=1} = {}",
    "({x=1}.abc)",
    "x > (0, {a = b} );",
    // declarations
    "var x = 0 + {a=1} = {}",
    "let o = {x=1};",
    "var j = {x=1};",
    "var j = {x={y=1}}={};",
    "const z = {x=1};",
    "const z = {x={y=1}}={};",
    "const {x=1};",
    "const {x={y=33}}={};",
    "var {x=1};",
    "let {x=1};",
    "let x, y, {z=1}={}, {w=2}, {e=3};",
    // array initialization
    "[{x=1, y = ({z=2} = {})}];",
    // try/catch
    "try {throw 'a';} catch ({x={y=1}}) {}",
    // if/else
    "if ({k: 1, x={y=2}={}}) {}",
    "if (false) {} else if (true) { ({x=1}) }",
    // switch
    "switch ('c') { case 'c': ({x=1}); }",
    // for
    "for ({x=1}; 1;) {1}",
    "for ({x={y=2}}; 1;) {1}",
    "for (var x = 0; x < 2; x++) { ({x=1, y=2}) }",
    "for (let x=1;{x=1};){}",
    "for (let x=1;{x={y=2}};){}",
    "for (let x=1;1;{x=1}){}",
    "for (let x=1;1;{x={y=2}}){}",
    // while
    "while ({x=1}) {1};",
    "while ({x={y=2}}={}) {1};",
    // with
    "with ({x=1}) {};",
    "with ({x={y=3}={}}) {};",
    "with (Math) { ({x=1}) };",
    // ternary
    "true ? {x=1} : 1;",
    "false ? 1 : {x=1};",
    "{x=1} ? 2 : 3;",
]

for (var stmt of SYNTAX_ERROR_STMTS) {
    assertThrowsInstanceOf(() => {
        eval(stmt);
    }, SyntaxError);
}

const REFERENCE_ERROR_STMTS = [
    "({x} += {});",
    "({x = 1}) = {x: 2};",
]

for (var stmt of REFERENCE_ERROR_STMTS) {
    assertThrowsInstanceOf(() => {
        eval(stmt);
    }, ReferenceError);
}

// A few tricky but acceptable cases:
// see https://bugzilla.mozilla.org/show_bug.cgi?id=932080#c2

assertEq((({a = 0}) => a)({}), 0);
assertEq((({a = 0} = {}) => a)({}), 0);
assertEq((({a = 0} = {}) => a)({a: 1}), 1);

{
    let x, y;
    ({x=1} = {});
    assertEq(x, 1);
    ({x=1} = {x: 4});
    assertEq(x, 4);
    ({x=1, y=2} = {})
    assertEq(x, 1);
    assertEq(y, 2);
}

{
    let {x={i=1, j=2}={}}={};
    assertDeepEq(x, ({}));
    assertEq(i, 1);
    assertEq(j, 2);
}

// Default destructuring values, which are variables, should be defined
// within closures (Bug 1255167).
{
    let f = function(a){
        return (function({aa = a}){ return aa; })({});
    };
    assertEq(f(9999), 9999);
}

if (typeof reportCompare == "function")
    reportCompare(true, true);
