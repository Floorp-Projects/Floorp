// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.3.29 Tuple.prototype.toSorted ( compareFn )
Except as described below, implements the same requirements as those of Array.prototype.sort as defined in 22.1.3.27. The implementation may be optimized with the knowledge that the this value is an object that has a fixed length and whose integer-indexed properties are not sparse.

Upon entry, the following steps are performed to initialize evaluation of the sorted function. These steps are used instead of the entry steps in 22.1.3.27:

1. If comparefn is not undefined and IsCallable(comparefn) is false, throw a TypeError exception.
2. Let T be ? thisTupleValue(this value).
3. Let list be a new List containing the elements of T.[[Sequence]].
4. Let len be the length of list.
A new Tuple containing the elements from the original Tuple is returned, where the elements are sorted. The sort order is the ordering, after completion of this function, of the elements of list. The sort order of the result of sorted is the same order as if the elements of list were sorted in an Array via %Array.prototype.sort% with comparefn as the first argument.

*/
/* Step 2 */
/* toSorted() should throw on a non-Tuple */
let method = Tuple.prototype.toSorted;
assertEq(method.call(#[6,5,4,3,2,1]), #[1,2,3,4,5,6]);
assertEq(method.call(Object(#[6,5,4,3,2,1])), #[1,2,3,4,5,6]);
assertThrowsInstanceOf(() => method.call("monkeys"), TypeError,
                       "value of TupleObject must be a Tuple");

/* Test that length is still handled correctly if it's overridden */
Object.defineProperty(Tuple.prototype, "length", { get() { return 0 } })
assertEq(#[5,4,3,2,1].toSorted(), #[1,2,3,4,5]);

/* Step 1. If comparefn is not undefined... */
assertEq(#[5,4,3,2,1].toSorted(), #[1,2,3,4,5]);
assertEq(#[5,4,3,2,1].toSorted(undefined), #[1,2,3,4,5]);
assertThrowsInstanceOf(() => #[1].toSorted("monkeys"), TypeError,
                       "bad comparefn passed to toSorted");
let comparefn = (x, y) => x < y;
assertEq(#[1,2,3,4,5].toSorted(comparefn), #[5,4,3,2,1]);

reportCompare(0, 0);

