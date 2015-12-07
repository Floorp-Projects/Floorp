/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

let length;
let iterable = {
   [Symbol.iterator]() { return this; },
   next() { length = arguments.length; return {done: true}; }
};

new Map(iterable);
// ensure no arguments are passed to next() during construction (Bug 1197095)
assertEq(length, 0);

let typeofThis;
Object.defineProperty(Number.prototype, Symbol.iterator, {
  value() {
    "use strict";
    typeofThis = typeof this;
    return { next() { return {done: true}; } };
  }
});

new Map(0);
// ensure that iterable objects retain their type (Bug 1197094)
assertEq(typeofThis, "number");

if (typeof reportCompare === "function")
  reportCompare(0, 0);
