// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.2.3 Tuple.of ( ...items )
The of method takes any number of arguments, and performs the following steps:

1. Let items be the List of arguments passed to this function.
2. For each element e of items,
a. If Type(e) is Object, throw a TypeError exception.
3. Let tuple be a new Tuple value whose [[Sequence]] is items.
4. Return tuple.
*/

assertEq(Tuple.of(), #[]);
assertEq(Tuple.of(undefined), #[undefined]);
assertEq(Tuple.of(null), #[null]);
assertEq(Tuple.of(1), #[1]);
assertEq(Tuple.of(1, 2), #[1,2]);
assertEq(Tuple.of(true, 5, "monkeys", #[3, 4]), #[true, 5, "monkeys", #[3, 4]]);
assertEq(Tuple.of(undefined, false, null, undefined), #[undefined, false, null, undefined]);

/* Step 2a. */
assertThrowsInstanceOf(() => Tuple.of([1, 2, 3]), TypeError,
                       "Tuple can't contain Object");
assertThrowsInstanceOf(() => Tuple.of([]), TypeError,
                       "Tuple can't contain Object");
assertThrowsInstanceOf(() => Tuple.of(new Object(), [1, 2, 3], new String("a")), TypeError,
                       "Tuple can't contain Object");

reportCompare(0, 0);
