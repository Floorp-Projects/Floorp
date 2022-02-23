// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/* Tuples should have a length ownProperty that can't be overridden, which will
 * be read by any built-in methods called on Tuples.
 * This test is expected to fail until the spec change in
 * https://github.com/tc39/proposal-record-tuple/issues/282 is implemented.
 */

/*
t = #[1,2,3];
Object.defineProperty(Tuple.prototype, "length", {value: 0});
assertEq(t.includes(2), true);
*/

reportCompare(0, 0);
