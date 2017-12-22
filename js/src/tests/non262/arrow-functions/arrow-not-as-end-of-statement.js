/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Grab-bag of ArrowFunctions with block bodies appearing in contexts where the
// subsequent token-examination must use the Operand modifier to avoid an
// assertion.

assertEq(`x ${a => {}} z`, "x a => {} z");

for (a => {}; ; )
  break;

for (; a => {}; )
  break;

for (; ; a => {})
  break;

Function.prototype[Symbol.iterator] = function() { return { next() { return { done: true }; } }; };
for (let m of 0 ? 1 : a => {})
  assertEq(true, false);
for (let [m] of 0 ? 1 : a => {})
  assertEq(true, false);
delete Function.prototype[Symbol.iterator];

for (let w in 0 ? 1 : a => {})
  break;
for (let [w] in 0 ? 1 : a => {})
  break;

function* stargen()
{
  yield a => {}
  /Q/g;

  var first = true;
  Function.prototype[Symbol.iterator] = function() {
    return {
      next() {
        var res = { done: true, value: 8675309 };
        if (first)
        {
          res = { value: "fnord", done: false };
          first = false;
        }

        return res;
      }
    };
  };


  yield* a => {}
  /Q/g;

  delete Function.prototype[Symbol.iterator];

  yield 99;
}
var gen = stargen();
assertEq(typeof gen.next().value, "function");
var result = gen.next();
assertEq(result.value, "fnord");
assertEq(result.done, false);
assertEq(gen.next().value, 99);
assertEq(gen.next().done, true);

switch (1)
{
  case a => {}:
   break;
}

assertEq(0[a => {}], undefined);

class Y {};
class X extends Y { constructor() { super[a => {}](); } };

if (a => {})
  assertEq(true, true);
else
  assertEq(true, false);

switch (a => {})
{
}

with (a => {});

assertEq(typeof (a => {}), "function");
assertEq(typeof (a => b => {}), "function");

for (var x in y => {})
  continue;

assertEq(eval("a => {}, 17, 42;"), 42);
assertEq(eval("42, a => {}, 17;"), 17);
assertEq(typeof eval("17, 42, a => {};"), "function");

assertEq(eval("1 ? 0 : a => {}, 17, 42;"), 42);
assertEq(eval("42, 1 ? 0 : a => {}, 17;"), 17);
assertEq(eval("17, 42, 1 ? 0 : a => {};"), 0);

var z = { x: 0 ? 1 : async a => {} };
assertEq(typeof z.x, "function");

var q = 0 ? 1 : async () => {};
assertEq(typeof q, "function");

var m = 0 ? 42 : m = foo => {} // ASI
/Q/g;
assertEq(typeof m, "function");

var { q: w = 0 ? 1 : a => {} } = {};
assertEq(typeof w, "function");

Function.prototype.c = 42;
var { c } = 0 ? 1 : a => {} // ASI
/Q/g;
assertEq(c, 42);

var c = 0 ? 1 : a => {}
/Q/g;
assertEq(typeof c, "function");
delete Function.prototype.c;

assertEq(typeof eval(0 ? 1 : a => {}), "function");

var zoom = 1 ? a => {} : 357;
assertEq(typeof zoom, "function");

var { b = 0 ? 1 : a => {} } = {};
assertEq(typeof b, "function");

var [k = 0 ? 1 : a => {}] = [];
assertEq(typeof k, "function");

assertEq(typeof [0 ? 1 : a => {}][0], "function");

Function.prototype[Symbol.iterator] = function() { return { next() { return { done: true }; } }; };
assertEq([...0 ? 1 : a => {}].length, 0);
delete Function.prototype[Symbol.iterator];

var props = Object.getOwnPropertyNames({ ...0 ? 1 : a => {} }).sort();
assertEq(props.length, 0);

function f1(x = 0 ? 1 : a => {}) { return x; }
assertEq(typeof f1(), "function");
assertEq(f1(5), 5);

var g1 = (x = 0 ? 1 : a => {}) => { return x; };
assertEq(typeof g1(), "function");
assertEq(g1(5), 5);

var h1 = async (x = 0 ? 1 : a => {}) => { return x; };
assertEq(typeof h1, "function");

function f2(m, x = 0 ? 1 : a => {}) { return x; }
assertEq(typeof f2(1), "function");
assertEq(f2(1, 5), 5);

var g2 = (m, x = 0 ? 1 : a => {}) => { return x; };
assertEq(typeof g2(1), "function");
assertEq(g2(1, 5), 5);

var h2 = async (m, x = 0 ? 1 : a => {}) => { return x; };
assertEq(typeof h2, "function");

function f3(x = 0 ? 1 : a => {}, q) { return x; }
assertEq(typeof f3(), "function");
assertEq(f3(5), 5);

var g3 = (x = 0 ? 1 : a => {}, q) => { return x; };
assertEq(typeof g3(), "function");
assertEq(g3(5), 5);

var h3 = async (x = 0 ? 1 : a => {}, q) => { return x; };
assertEq(typeof h3, "function");

var asyncf = async () => {};
assertEq(typeof asyncf, "function");

var { [0 ? 1 : a => {}]: h } = { "a => {}": "boo-urns!" };
assertEq(h, "boo-urns!");

if (typeof reportCompare === "function")
  reportCompare(true, true);
