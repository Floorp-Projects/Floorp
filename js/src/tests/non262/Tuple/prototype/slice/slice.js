// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.3.5 Tuple.prototype.slice ( start, end )
When the slice method is called with two arguments, start and end, and returns a Tuple containing the elements of the Tuple from element start up to, but not including, element end (or through the end of the tuple if end is undefined). If start is negative, it is treated as length + start where length is the length of the tuple. If end is negative, it is treated as length + end where length is the length of the tuple.

The following steps are taken:

1. Let T be ? thisTupleValue(this value).
2. Let list be T.[[Sequence]].
3. Let len be the number of elements in list.
4. Let relativeStart be ? ToInteger(start).
5. If relativeStart < 0, let k be max((len + relativeStart), 0); else let k be min(relativeStart, len).
6. If end is undefined, let relativeEnd be len; else let relativeEnd be ? ToInteger(end).
7. If relativeEnd < 0, let final be max((len + relativeEnd), 0); else let final be min(relativeEnd, len).
8. Let newList be a new empty List.
9. Repeat, while k < final,
a. Let kValue be list[k].
b. Assert: Type(kValue) is not Object.
c. Append kValue to the end of newList.
d. Set k to k + 1.
10. Return a new Tuple value whose [[Sequence]] is newList.
*/

/* Step 1 */
/* slice() should throw on a non-Tuple */
let method = Tuple.prototype.slice;
assertEq(method.call(#[1,2,3,4,5,6], 2, 4), #[3,4]);
assertEq(method.call(Object(#[1,2,3,4,5,6]), 2, 4), #[3,4]);
assertThrowsInstanceOf(() => method.call("monkeys"), TypeError,
                       "value of TupleObject must be a Tuple");
assertThrowsInstanceOf(() => method.call(true), TypeError,
                       "value of TupleObject must be a Tuple");
assertThrowsInstanceOf(() => method.call(false), TypeError,
                       "value of TupleObject must be a Tuple");


let tup = #[1,2,3];
let len = 3;
/* Step 4 -- toInteger returns 0 for non-integers */
assertEq(tup.slice("monkeys", 4), tup.slice(0, 4));
assertThrowsInstanceOf(() => tup.slice(Symbol("1"), 4),
                       TypeError, "can't convert symbol to number");
assertEq(tup.slice(undefined, 4), tup.slice(0, 4));
assertEq(tup.slice(undefined), tup.slice(0));
assertEq(tup.slice(NaN), tup.slice(0));
assertEq(tup.slice(Number.POSITIVE_INFINITY), tup.slice(len));
assertEq(tup.slice(Number.NEGATIVE_INFINITY), tup.slice(0));
assertEq(tup.slice({valueOf: function() { return 0 },
                    toString: function() { return 3 }}),
         tup.slice(0, 3));
assertEq(tup.slice(), tup.slice(0));


/* Step 5 -- if relativeStart < 0, k should be max((len + relativeStart), 0) */
/* Case 1: len + relativeStart > 0 */
var relativeStart = -2;
assertEq(tup.slice(relativeStart, 4), tup.slice(len + relativeStart, 4));
/* Case 2: len + relativeStart === 0 */
relativeStart = (-1 * len);
assertEq(tup.slice(relativeStart, 4), tup.slice(0, 4));
/* Case 3: len + relativeStart < 0 */
relativeStart = -256;
assertEq(tup.slice(relativeStart, 4), tup.slice(0, 4));

/* Step 5 -- start positive, end positive */
var tup2 = #[1,2,3,4,5,6,7];
relativeStart = 2;
/* length > end > start */
assertEq(tup2.slice(relativeStart, 4), #[3,4]);
/* start == len */
/* length == end == start */
assertEq(tup2.slice(tup2.length, tup2.length), #[]);
/* start > len */
/* start > end == len */
assertEq(tup2.slice(tup2.length + 5, tup2.length), tup2.slice(tup2.length, tup2.length));
/* length > end > start = 0 */
relativeStart = 0;
assertEq(tup2.slice(relativeStart, 4), #[1,2,3,4]);
/* length > end == start > 0 */
relativeStart = 2;
assertEq(tup2.slice(relativeStart, relativeStart), #[]);
/* length > start > end > 0 */
relativeStart = 4;
relativeEnd = 2;
assertEq(tup2.slice(relativeStart, relativeEnd), #[]);
/* length = end > start > 0 */
relativeEnd = tup2.length;
assertEq(tup2.slice(relativeStart, relativeEnd), #[5,6,7]);
/* end > length > start > 0 */
relativeEnd = tup2.length + 3;
assertEq(tup2.slice(relativeStart, relativeEnd), #[5,6,7]);

/* length > end == abs(start), start < 0 */
relativeStart = -4;
relativeEnd = 4;
assertEq(tup2.slice(relativeStart, relativeEnd), #[4]);
/* length == end > abs(start), start < 0 */
relativeEnd = tup2.length;
assertEq(tup2.slice(relativeStart, relativeEnd), #[4,5,6,7]);
/* abs(start) = length > end > 0, start < 0 */
relativeStart = -1*tup2.length;
relativeEnd = 5;
assertEq(tup2.slice(relativeStart, relativeEnd), #[1,2,3,4,5]);
/* abs(start) > length = end > 0 */
relativeStart = (-1*tup2.length) - 4;
relativeEnd = tup2.length;
assertEq(tup2.slice(relativeStart, relativeEnd), tup2);

/* Step 6 -- if end is undefined, let relativeEnd = len */
assertEq(tup.slice(2, undefined), tup.slice(2, len));
assertEq(tup.slice(2), tup.slice(2, len));
/* relativeEnd converted with toInteger */
assertEq(tup.slice(2, "monkeys"), tup.slice(2, 0));
assertThrowsInstanceOf(() => tup.slice(2, Symbol("x")),
                      TypeError, "can't convert Symbol to Number");
assertEq(tup.slice(2, 2.5), tup.slice(2, 0));
assertEq(tup.slice(2, NaN), tup.slice(2, 0));
assertEq(tup.slice(2, Number.POSITIVE_INFINITY), tup.slice(2, len));
assertEq(tup.slice(2, Number.NEGATIVE_INFINITY), tup.slice(2, 0));
assertEq(tup.slice(0, Number.POSITIVE_INFINITY), tup.slice(0, len));
assertEq(tup.slice(0, Number.NEGATIVE_INFINITY), tup.slice(0, 0));
assertEq(tup.slice({valueOf: function() { return 3 },
                    toString: function() { return 0 }}),
         tup.slice(0, 0));

/* Step 7 -- if relativeEnd < 0, final should be max((len + relativeEnd), 0) */
/* Case 1: len + relativeEnd > 0 */
var relativeEnd = -1;
relativeStart = 2;
assertEq(tup.slice(relativeStart, relativeEnd), tup.slice(relativeStart, len + relativeEnd));
/* Case 2: len + relativeEnd === 0 */
relativeEnd = (-1 * len);
assertEq(tup.slice(relativeStart, relativeEnd), tup.slice(relativeStart, 0));
/* Case 3: len + relativeEnd < 0 */
relativeEnd = -256;
assertEq(tup.slice(relativeStart, relativeEnd), tup.slice(relativeStart, 0));
/* start and end both negative */
relativeStart = -3;
relativeEnd = -1;
assertEq(tup2.slice(relativeStart, relativeEnd), #[5, 6]);
relativeEnd = (-1*len);
assertEq(tup2.slice(relativeStart, relativeEnd), #[]);
relativeEnd = -256;
assertEq(tup2.slice(relativeStart, relativeEnd), #[]);

/* length > abs(end) > start === 0 */
relativeStart = 0;
relativeEnd = -3;
assertEq(tup2.slice(relativeStart, relativeEnd), #[1,2,3,4]);
/* length > abs(end) > start > 0 */
relativeStart = 2;
assertEq(tup2.slice(relativeStart, relativeEnd), #[3,4]);
/* length == abs(end) > start == 0 */
relativeEnd = -1*tup2.length;
assertEq(tup2.slice(relativeStart, relativeEnd), #[]);
/* abs(end) > length > start > 0 */
relativeEnd = (-1*tup2.length) - 5;
assertEq(tup2.slice(relativeStart, relativeEnd), #[]);
/* -length == start < end < 0 */
relativeStart = -1*tup2.length;
relativeEnd = 5;
assertEq(tup2.slice(relativeStart, relativeEnd), #[1,2,3,4,5]);
/* -length < start < end < 0 */
relativeStart = -3;
relativeEnd = -1;
assertEq(tup2.slice(relativeStart, relativeEnd), #[5,6]);
/* start < -length < end < 0 */
relativeStart = (-1*tup2.length) - 5;
relativeEnd = -3;
assertEq(tup2.slice(relativeStart, relativeEnd), #[1,2,3,4]);
/* start = end < -length */
relativeEnd = relativeStart;
assertEq(tup2.slice(relativeStart, relativeEnd), #[]);

reportCompare(0, 0);

