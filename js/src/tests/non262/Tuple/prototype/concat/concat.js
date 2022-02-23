// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.3.6 Tuple.prototype.concat ( ...args )

When the concat method is called with zero or more arguments, it returns a Tuple containing the elements of the Tuple followed by the elements of each argument in order.

The following steps are taken:

1. Let T be ? thisTupleValue(this value).
2. Let list be a new empty List.
3. Let n be 0.
4. Let items be a List whose first element is T and whose subsequent element are, in left to right order, the arguments that were passed to this function invocation.
5. Repeat, while items is not empty,
a. Remove the first element from items and let E be the value of the element.
b. Let spreadable be ? IsConcatSpreadable(E).
c. If spreadable is true, then
i. Let k be 0.
ii. Let len be ? LengthOfArrayLike(E).
iii. If n + len > 253 - 1, throw a TypeError exception.
iv. Repeat, while k < len,
1. Let P be ! ToString(k).
2. Let exists be ? HasProperty(E, P).
3. If exists is true, then
a. Let subElement be ? Get(E, P).
b. If Type(subElement) is Object, throw a TypeError exception.
c. Append subElement to the end of list list.
4. Set n to n + 1.
5. Set k to k + 1.
d. Else,
i. NOTE: E is added as a single item rather than spread.
ii. If n ≥ 253 - 1, throw a TypeError exception.
iii. If Type(E) is Object, throw a TypeError exception.
iv. Append E to the end of list list.
v. Set n to n + 1.
6. Return a new Tuple value whose [[Sequence]] is list.
*/

/* Step 1 */
/* concat() should throw on a non-Tuple */
let method = Tuple.prototype.concat;
assertEq(method.call(#[1,2,3,4,5,6], #[1],#[2,3]), #[1,2,3,4,5,6,1,2,3]);
assertEq(method.call(Object(#[1,2,3,4,5,6]), #[1],#[2,3]), method.call(#[1,2,3,4,5,6],#[1],#[2,3]));
assertThrowsInstanceOf(() => method.call("monkeys"), TypeError,
                       "value of TupleObject must be a Tuple");

/* Step 5 */
/* No arguments or empty arguments => returns this */
let tup = #[1,2,3];
assertEq(tup.concat(), tup);
assertEq(tup.concat(#[]), tup);
assertEq(tup.concat([]), tup);
assertEq(tup.concat(#[],#[],#[]), tup);

/* Step 5 */
/* 1 spreadable argument */
assertEq(tup.concat(tup), #[1,2,3,1,2,3]);
assertEq(tup.concat([1,2,3]), #[1,2,3,1,2,3]);

/* spreadable followed by non-spreadable */
assertEq(tup.concat(tup, "monkeys"), #[1,2,3,1,2,3, "monkeys"]);
assertEq(tup.concat([1,2,3], "monkeys"), #[1,2,3,1,2,3, "monkeys"]);

/* non-spreadable followed by spreadable */
assertEq(tup.concat("monkeys", tup), #[1,2,3, "monkeys", 1,2,3]);
assertEq(tup.concat("monkeys", [1,2,3]), #[1,2,3, "monkeys", 1,2,3,]);

/* Step 5.c.iii.
   If n + len > 2^53 - 1, throw a TypeError */
var spreadableLengthOutOfRange = {};
spreadableLengthOutOfRange.length = Number.MAX_SAFE_INTEGER;
spreadableLengthOutOfRange[Symbol.isConcatSpreadable] = true;
/*
TODO
this.length = (2^53/2)
this.concat(a) throws if a.length = 2^53/2 - 1
*/


assertThrowsInstanceOf(() => tup.concat(spreadableLengthOutOfRange),
                       TypeError,
                       'Too long array');

/* Step 5.c.iv.2. Let exists be ?HasProperty(E, P)
   Step 5.c.iv.3 Append only if exists is true */
assertEq(tup.concat([42,17,,5,,12]), #[1,2,3,42,17,5,12]);

/* Step 5.c.iv.3.b. -- Object elements should throw */
assertThrowsInstanceOf(() => tup.concat([1, new Object(), 2]),
                       TypeError,
                       '#[1,2,3].concat([1, new Object(), 2]) throws a TypeError exception');

/* Step 5d -- add as a single item */
assertEq(tup.concat("monkeys"), #[1,2,3, "monkeys"]);
/*
Step 5d.c.ii.
- Throw if this has length 2^53
TODO: not sure how to test this -- creating a tuple with length 2^53 takes too long
*/

/* Step 5d.iii -- Object should throw */
assertThrowsInstanceOf(() => tup.concat(new Object()), TypeError,
                       '#[1,2,3].concat(new Object()) throws a TypeError exception');

reportCompare(0, 0);
