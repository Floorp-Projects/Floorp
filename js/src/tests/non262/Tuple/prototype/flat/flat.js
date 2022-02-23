// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.3.16 Tuple.prototype.flat ( [ depth ] )
When the flat method is called with zero or one arguments, the following steps are taken:

1. Let T be ? thisTupleValue(this value).
2. Let list be T.[[Sequence]].
3. Let depthNum be 1.
4. If depth is not undefined, then
a. Set depthNum to ? ToInteger(depth).
5. Let flat be a new empty List.
6. Perform ? FlattenIntoTuple(flat, list, depthNum).
7. Return a new Tuple value whose [[Sequence]] is flat.

8.2.3.16.1 FlattenIntoTuple ( target, source, depth [ , mapperFunction, thisArg ] )
The abstract operation FlattenIntoTuple takes arguments target, source, and depth and optional arguments mapperFunction and thisArg. It performs the following steps when called:

1. Assert: target is a List.
2. Assert: source is a List.
3. Assert: ! IsInteger(depth) is true, or depth is either +∞ or -∞.
4. Assert: If mapperFunction is present, then ! IsCallable(mapperFunction) is true, thisArg is present, and depth is 1.
5. Let sourceIndex be 0.
6. For each element of source,
a. If mapperFunction is present, then
i. Set element to ? Call(mapperFunction, thisArg, « element, sourceIndex, source »).
ii. If Type(element) is Object, throw a TypeError exception.
b. If depth > 0 and Type(element) is Tuple, then
i. Perform ? FlattenIntoTuple(target, element, depth - 1).
c. Else,
i. Let len be the length of target.
ii. If len ≥ 253 - 1, throw a TypeError exception.
iii. Append element to target.
d. Set sourceIndex to sourceIndex + 1.

*/
/* Step 1 */
/* flat() should throw on a non-Tuple */
let method = Tuple.prototype.flat;
assertEq(method.call(#[1,#[2],3]), #[1,2,3]);
assertEq(method.call(Object(#[1,#[2],3])), #[1,2,3]);
assertThrowsInstanceOf(() => method.call("monkeys"), TypeError,
                       "value of TupleObject must be a Tuple");
assertThrowsInstanceOf(() => method.call(null), TypeError,
                       "value of TupleObject must be a Tuple");
assertThrowsInstanceOf(() => method.call(), TypeError,
                       "value of TupleObject must be a Tuple");


let tup = #[1,2,#[3,#[4,5],6],#[5,6],7];
let tup2 = #[1,2,#[3,#[4,#["a", "b"], 5],6],#[5,#[6,#[7,8,#[9,10]]]],7];

/* Step 3 -- depth is converted to Integer */
assertEq(tup.flat("monkeys"), tup.flat(0));
assertEq(tup.flat({}), tup.flat(0));
assertEq(tup.flat(+0), tup.flat(0));
assertEq(tup.flat(-0), tup.flat(0));
assertEq(tup.flat('2'), tup.flat(2));
assertEq(tup.flat(true), tup.flat(1));
assertEq(tup.flat(false), tup.flat(0));
assertEq(tup.flat(null), tup.flat(0));
assertEq(tup.flat(NaN), tup.flat(0));
assertEq(tup.flat([1,2,3]), tup.flat(0));
assertThrowsInstanceOf(() => tup.flat(Symbol("x")), TypeError,
                       "can't convert symbol to number");
assertThrowsInstanceOf(() => tup.flat(Object.create(null)), TypeError,
                       "can't convert Object to number");
assertThrowsInstanceOf(() => tup.flat(#[1]), TypeError,
                       "can't convert Tuple to number");


/* Step 3 -- if depth is undefined, depthNum is set to 1 */
assertEq(tup.flat(undefined), tup.flat(1));
assertEq(tup.flat(), tup.flat(1));

/* Step 7 */
assertEq(#[].flat(), #[]);
assertEq(#[1].flat(), #[1]);
assertEq(#[#[1,2],#[3,4]].flat(), #[1,2,3,4]);
assertEq(tup.flat(0), tup);
assertEq(tup.flat(1), #[1,2,3,#[4,5],6,5,6,7]);
assertEq(tup.flat(2), #[1,2,3,4,5,6,5,6,7]);
assertEq(tup.flat(3), tup.flat(2));
assertEq(tup2.flat(0), tup2);
assertEq(tup2.flat(1), #[1,2,3,#[4,#["a", "b"], 5],6,5,#[6,#[7,8,#[9,10]]],7]);
assertEq(tup2.flat(2), #[1,2,3,4,#["a", "b"],5,6,5,6,#[7, 8, #[9, 10]],7]);
assertEq(tup2.flat(3), #[1,2,3,4,"a","b",5,6,5,6, 7, 8, #[9, 10], 7]);
assertEq(tup2.flat(4), #[1,2,3,4,"a","b",5,6,5,6, 7, 8, 9, 10, 7]);

/* FlattenIntoTuple steps: */
/* Step 3: depth can be Infinity or -Infinity */
assertEq(tup2.flat(Infinity), tup2.flat(4));
assertEq(tup2.flat(-Infinity), tup2.flat(0));

/* Step 6.c.ii. -- throw if len would be > n^253 - 1 -- not sure how to test this */

reportCompare(0, 0);
