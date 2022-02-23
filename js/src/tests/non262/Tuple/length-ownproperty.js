// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/* Tuples should have a length ownProperty that can't be overridden
 * This test is expected to fail until the spec change in
 * https://github.com/tc39/proposal-record-tuple/issues/282 is implemented.
 */

/*
var desc = Object.getOwnPropertyDescriptor(#[1,2,3], "length");
assertEq(desc.value, 3);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, false);
Object.defineProperty(Tuple.prototype, "length", {value: 0});
desc = Object.getOwnPropertyDescriptor(#[1,2,3], "length");
assertEq(desc.value, 3);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, false);
*/

reportCompare(0, 0);
