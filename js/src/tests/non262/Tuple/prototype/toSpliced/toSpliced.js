// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.3.30 Tuple.prototype.toSpliced ( start, deleteCount, ...items )
When the toSpliced method is called with two or more arguments start, deleteCount and zero or more items, a new Tuple is returned where the deleteCount elements of the Tuple starting at integer index start are replaced by the arguments items.

The following steps are taken:

1. Let T be ? thisTupleValue(this value).
2. Let list be T.[[Sequence]].
3. Let len be the number of elements in list.
4. Let relativeStart be ? ToInteger(start).
5. If relativeStart < 0, let actualStart be max((len + relativeStart), 0); else let actualStart be min(relativeStart, len).
6. If the number of actual arguments is 0, then
a. Let insertCount be 0.
b. Let actualDeleteCount be 0.
7. Else if the number of actual arguments is 1, then
a. Let insertCount be 0.
b. Let actualDeleteCount be len - actualStart.
8. Else,
a. Let insertCount be the number of actual arguments minus 2.
b. Let dc be ? ToInteger(deleteCount).
c. Let actualDeleteCount be min(max(dc, 0), len - actualStart).
9. If len + insertCount - actualDeleteCount > 253 - 1, throw a TypeError exception.
10. Let k be 0.
11. Let items be a List whose elements are, in left to right order, the portion of the actual argument list starting with the third argument. The list is empty if fewer than three arguments were passed.
12. Let itemCount be the number of elements in items.
13. Let newList be a new empty List.
14. Repeat, while k < actualStart,
a. Let E be list[k].
b. Append E to the end of list newList.
c. Set k to k + 1.
15. Let itemK be 0.
16. Repeat, while itemK < itemCount.
a. Let E be items[itemK].
b. If Type(E) is Object, throw a TypeError exception.
c. Append E to the end of newList.
d. Set itemK to itemK + 1.
17. Set k to actualStart + actualDeleteCount.
18. Repeat, while k < len,
a. Let E be list[k].
b. Append E to the end of newList.
c. Set k to k + 1.
19. Return a new Tuple value whose [[Sequence]] is newList.
*/
/* Step 1 */
/* toSpliced() should throw on a non-Tuple */
let method = Tuple.prototype.toSpliced;
assertEq(method.call(#[1,2,3,4,5,6], 2, 3), #[1,2,6]);
assertEq(method.call(Object(#[1,2,3,4,5,6]),2,3), #[1,2,6]);
assertThrowsInstanceOf(() => method.call("monkeys"), TypeError,
                       "value of TupleObject must be a Tuple");
assertThrowsInstanceOf(() => method.call(true), TypeError,
                       "value of TupleObject must be a Tuple");
assertThrowsInstanceOf(() => method.call(false), TypeError,
                       "value of TupleObject must be a Tuple");

/* method does not access constructor */
var t = #[];
t.constructor = null;
var result = t.toSpliced(0, 0);
assertEq(result, #[]);
assertEq(Object.getPrototypeOf(result), Tuple.prototype);

/* Step 3 */
/* Test that length is still handled correctly if it's overridden */
Object.defineProperty(Tuple.prototype, "length", { get() { return 0 } })
assertEq(#[1,2,3,4,5].toSpliced(1, 2), #[1,4,5]);

var tup = #[1,2,3];
var len = 3;
/* Step 4 -- toInteger returns 0 for non-integers */
assertEq(tup.toSpliced("monkeys", 2), tup.toSpliced(0, 2));
assertEq(tup.toSpliced(undefined, 2), tup.toSpliced(0, 2));
assertEq(tup.toSpliced(undefined), tup.toSpliced(0, len));
assertEq(tup.toSpliced(), tup.slice(0));

/* Step 5 */
/* relativeStart < 0, len + relativeStart > 0 */
var relativeStart = -1;
var result = tup.toSpliced(relativeStart, 1);
assertEq(result, tup.toSpliced(len + relativeStart, 1));
assertEq(result, #[1,2]);
/* relativeStart < 0, len + relativeStart < 0 */
relativeStart = -4;
result = tup.toSpliced(relativeStart, 1);
assertEq(result, tup.toSpliced(0, 1));
assertEq(result, #[2,3]);
/* relativeStart < 0, len + relativeStart === 0 */
relativeStart = -3;
result = tup.toSpliced(relativeStart, 1);
assertEq(result, tup.toSpliced(0, 1));
/* relativeStart === 0 */
relativeStart = 0;
result = tup.toSpliced(relativeStart, 2);
assertEq(result, #[3]);
/* relativeStart > 0, relativeStart < len */
relativeStart = 2;
result = tup.toSpliced(relativeStart, 1);
assertEq(result, #[1,2]);
/* relativeStart > 0, relativeStart > len */
relativeStart = 5;
result = tup.toSpliced(relativeStart, 1);
assertEq(result, tup.toSpliced(len, 1));
assertEq(result, tup);
/* relativeStart > 0, relativeStart === len */
relativeStart = len;
result = tup.toSpliced(relativeStart, 1);
assertEq(result, tup);

/* Step 6 - no arguments */
result = tup.toSpliced();
assertEq(result, tup.toSpliced(0, 0));
assertEq(result, tup);

/* Step 7 -- 1 argument */
result = tup.toSpliced(1);
assertEq(result, tup.toSpliced(1, len - 1));
assertEq(result, #[1]);

/* Step 8 */
/* 2 arguments */
/* insertCount = 0 */
/* (a) deleteCount = non-integer */
assertEq(tup.toSpliced(1, "monkeys"), tup.toSpliced(1, 0));
assertEq(tup.toSpliced(1, undefined), tup.toSpliced(1, 0));
assertEq(tup.toSpliced(1, true), tup.toSpliced(1, 1));
/* (b) deleteCount < 0, len - actualStart > 0 */
result = tup.toSpliced(1, -1);
assertEq(result, tup.toSpliced(1, 0));
assertEq(result, tup);
/* (c) deleteCount < 0, len - actualStart === 0 */
result = tup.toSpliced(3, -1);
assertEq(result, tup.toSpliced(3, 0));
assertEq(result, tup);
/* (d) deleteCount < 0, len - actualStart < 0 */
/* Step 5 guarantees this case can't happen */
/* (e) deleteCount === 0 */
assertEq(tup.toSpliced(1, 0), tup);
/* (f) deleteCount > 0, deleteCount < (len - actualStart) */
var tup2 = #[1,2,3,4,5];
var tup2_len = 5;
result = tup2.toSpliced(1, 3);
assertEq(result, #[1, 5]);
/* (g) deleteCount > 0, deleteCount > (len - actualStart) */
var actualStart = 3;
result = tup2.toSpliced(actualStart, 4);
assertEq(result, tup2.toSpliced(actualStart, tup2_len - actualStart));
assertEq(result, #[1,2,3]);
/* 3 arguments */
assertEq(tup2.toSpliced(1, "bunnies", "monkeys"), tup2.toSpliced(1, 0, "monkeys")); // (a)
assertEq(tup2.toSpliced(1, -1, "monkeys"), tup2.toSpliced(1, 0, "monkeys")); // (b)
assertEq(tup2.toSpliced(tup2_len, -1, "monkeys"), tup2.toSpliced(tup2_len, 0, "monkeys")); // (c)
assertEq(tup2.toSpliced(2, 0, "monkeys"), #[1, 2, "monkeys", 3, 4, 5]); // (e)
assertEq(tup2.toSpliced(1, 3, "monkeys"), #[1, "monkeys", 5]); // (f)
assertEq(tup2.toSpliced(3, 4, "monkeys"), #[1, 2, 3, "monkeys"]); // (g)
/* 4 arguments */
assertEq(tup2.toSpliced(1, "monkeys", 42, 17), tup2.toSpliced(1, 0, 42, 17)); // (a)
assertEq(tup2.toSpliced(1, -1, 42, 17), tup2.toSpliced(1, 0, 42, 17)); // (b)
assertEq(tup2.toSpliced(tup2_len, -1, 42, 17), tup2.toSpliced(tup2_len, 0, 42, 17)); // (c)
assertEq(tup2.toSpliced(2, 0, 42, 17), #[1, 2, 42, 17, 3, 4, 5]); // (e)
assertEq(tup2.toSpliced(1, 3, 42, 17), #[1, 42, 17, 5]); // (f)
assertEq(tup2.toSpliced(3, 4, 42, 17), #[1, 2, 3, 42, 17]); // (g)

/* Step 9 */
/* If len + insertCount - actualDeleteCount > 2^53 - 1, throw a TypeError exception.
   Not sure how to test the error case without constructing a tuple with length 2^53 */

/* Steps 14-18 */
/* each combination of:
  actualStart = 0, actualStart = 1, actualStart = 4
  itemCount = 0, itemCount = 1, itemCount = 4
  actualStart + actualDeleteCount = len, actualStart + actualDeleteCount < len */

/* actualStart = 0, itemCount = 0, actualDeleteCount = len */
var tup = #[1,2,3,4,5,6];
var len = 6;
var actualStart;
var actualDeleteCount;
actualStart = 0;
actualDeleteCount = len;
assertEq(tup.toSpliced(actualStart, actualDeleteCount), #[]);

/* actualStart = 0, itemCount = 1, actualDeleteCount = len */
actualStart = 0;
actualDeleteCount = len;
assertEq(tup.toSpliced(actualStart, actualDeleteCount, "monkeys"), #["monkeys"]);

/* actualStart = 0, itemCount = 4, actualDeleteCount = len */
assertEq(tup.toSpliced(actualStart, actualDeleteCount, "a", "b", "c", "d"), #["a", "b", "c", "d"]);

/* actualStart = 0, itemCount = 0, actualDeleteCount < len */
actualDeleteCount = 3;
assertEq(tup.toSpliced(actualStart, actualDeleteCount), #[4, 5, 6]);

/* actualStart = 0, itemCount = 1, actualDeleteCount < len */
assertEq(tup.toSpliced(actualStart, actualDeleteCount, "monkeys"), #["monkeys", 4, 5, 6]);

/* actualStart = 0, itemCount = 4, actualDeleteCount < len */
assertEq(tup.toSpliced(actualStart, actualDeleteCount, 42, 43, 44, 45), #[42, 43, 44, 45, 4, 5, 6]);

/* actualStart = 1, itemCount = 0, actualDeleteCount = len - 1 */
actualStart = 1;
actualDeleteCount = len - 1;
assertEq(tup.toSpliced(actualStart, actualDeleteCount), #[1]);

/* actualStart = 1, itemCount = 1, actualDeleteCount = len - 1 */
assertEq(tup.toSpliced(actualStart, actualDeleteCount, "monkeys"), #[1, "monkeys"]);

/* actualStart = 1, itemCount = 4, actualDeleteCount = len - 1 */
assertEq(tup.toSpliced(actualStart, actualDeleteCount, 0.0, 0.1, 0.2, 0.3), #[1, 0.0, 0.1, 0.2, 0.3]);

/* actualStart = 1, itemCount = 0, actualDeleteCount < len - 1 */
actualDeleteCount = 1;
assertEq(tup.toSpliced(actualStart, actualDeleteCount), #[1, 3, 4, 5, 6]);

/* actualStart = 1, itemCount = 1, actualDeleteCount < len - 1 */
assertEq(tup.toSpliced(actualStart, actualDeleteCount, "monkeys"), #[1, "monkeys", 3, 4, 5, 6]);

/* actualStart = 1, itemCount = 4, actualDeleteCount < len - 1 */
assertEq(tup.toSpliced(actualStart, actualDeleteCount, 8, 9, 10, 11), #[1, 8, 9, 10, 11, 3, 4, 5, 6]);

/* actualStart = 4, itemCount = 0, actualDeleteCount = len - 4 */
actualStart = 4;
actualDeleteCount = len - 4;
assertEq(tup.toSpliced(actualStart, actualDeleteCount), #[1, 2, 3, 4]);

/* actualStart = 4, itemCount = 1, actualDeleteCount = len - 4 */
assertEq(tup.toSpliced(actualStart, actualDeleteCount, "monkeys"), #[1, 2, 3, 4, "monkeys"]);

/* actualStart = 4, itemCount = 4, actualDeleteCount = len - 4 */
assertEq(tup.toSpliced(actualStart, actualDeleteCount, 0, -1, -2, -3), #[1, 2, 3, 4, 0, -1, -2, -3]);

/* actualStart = 4, itemCount = 0, actualDeleteCount < len - 4 */
actualDeleteCount = 1;
assertEq(tup.toSpliced(actualStart, actualDeleteCount), #[1, 2, 3, 4, 6]);

/* actualStart = 4, itemCount = 1, actualDeleteCount < len - 4 */
assertEq(tup.toSpliced(actualStart, actualDeleteCount, "monkeys"), #[1, 2, 3, 4, "monkeys", 6]);

/* actualStart = 4, itemCount = 4, actualDeleteCount < len - 4 */
assertEq(tup.toSpliced(actualStart, actualDeleteCount, "p", "q", "r", "s"), #[1, 2, 3, 4, "p", "q", "r", "s", 6]);

/* Step 16.b -- item is Object */
assertThrowsInstanceOf(() => tup.toSpliced(actualStart, actualDeleteCount, []),
                       TypeError, "Tuple can't contain Object");
assertThrowsInstanceOf(() => tup.toSpliced(actualStart, actualDeleteCount, [], 2, 3),
                       TypeError, "Tuple can't contain Object");
assertThrowsInstanceOf(() => tup.toSpliced(actualStart, actualDeleteCount, 2, new Object(), 3),
                       TypeError, "Tuple can't contain Object");
assertThrowsInstanceOf(() => tup.toSpliced(actualStart, actualDeleteCount, 2, 3, {"a": "b"}),
                       TypeError, "Tuple can't contain Object");


reportCompare(0, 0);
