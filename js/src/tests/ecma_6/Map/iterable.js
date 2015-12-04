/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

let length;
let iterable = {
   [Symbol.iterator]() { return this; },
   next() { length = arguments.length; return {done: true}; }
};

let m = new Map(iterable);
// ensure no arguments are passed to next() during construction (Bug 1197095)
assertEq(length, 0);

if (typeof reportCompare === "function")
  reportCompare(0, 0);
