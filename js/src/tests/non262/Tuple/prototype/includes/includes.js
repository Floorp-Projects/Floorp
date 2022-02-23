// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.3.7 Tuple.prototype.includes ( searchElement [ , fromIndex ] )
Tuple.prototype.includes is a distinct function that implements the same algorithm as Array.prototype.includes as defined in 22.1.3.13 except that ? thisTupleValue(this value) is used instead of directly accessing the this value. The implementation of the algorithm may be optimized with the knowledge that the this value is an object that has a fixed length and whose integer-indexed properties are not sparse, do not change, and their access is not observable. However, such optimization must not introduce any observable changes in the specified behaviour of the algorithm.

This function is not generic, since thisTupleValue(this value) can return an abrupt completion: in that case, that exception is thrown instead of evaluating the algorithm.
*/

/* Step 1 */
/* includes() should throw on a non-Tuple */
let method = Tuple.prototype.includes;
assertEq(method.call(#[1,2,3,4,5,6], 2), true);
assertEq(method.call(Object(#[1,2,3,4,5,6]), 2), true);
assertThrowsInstanceOf(() => method.call("monkeys", 2), TypeError,
                       "value of TupleObject must be a Tuple");

/* Not sure what else to test, since it just calls the array method */
reportCompare(0, 0);
