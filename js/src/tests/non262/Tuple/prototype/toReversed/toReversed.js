// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.3.28 Tuple.prototype.toReversed ( )
When the toReversed method is called, the following steps are taken:

1. Let T be ? thisTupleValue(this value).
2. Let oldList be a new List containing the elements of T.[[Sequence]].
3. Let newList be a new empty List.
4. Repeat, while oldList is not empty,
a. Remove the last element from oldList, and let E be the value of the element.
b. Append E to the end of List newList.
5. Return a new Tuple value whose [[Sequence]] is newList.
*/
/* Step 1 */
/* toReversed() should throw on a non-Tuple */
let method = Tuple.prototype.toReversed;
assertEq(method.call(#[1,2,3,4,5,6]), #[6,5,4,3,2,1]);
assertEq(method.call(Object(#[1,2,3,4,5,6])), #[6,5,4,3,2,1]);
assertThrowsInstanceOf(() => method.call("monkeys"), TypeError,
                       "value of TupleObject must be a Tuple");

assertEq(#[].toReversed(), #[]);
assertEq(#[1].toReversed(), #[1]);

/* Test that length is still handled correctly if it's overridden */
Object.defineProperty(Tuple.prototype, "length", { get() { return 0 } })
assertEq(#[1,2,3,4,5].toReversed(), #[5,4,3,2,1]);

reportCompare(0, 0);
