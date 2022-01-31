// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.3.20 Tuple.prototype.map ( callbackfn [ , thisArg ] )
NOTE
callbackfn should be a function that accepts three arguments. map calls callbackfn once for each element in the Tuple, in ascending order, and constructs a new tuple from the results.

If a thisArg parameter is provided, it will be used as the this value for each invocation of callbackfn. If it is not provided, undefined is used instead.

callbackfn is called with three arguments: the value of the element, the index of the element, and the Tuple being traversed.

When the map method is called with one or two arguments, the following steps are taken:

1. Let T be ? thisTupleValue(this value).
2. Let list be T.[[Sequence]].
3. Let len be the number of elements in list.
4. If IsCallable(callbackfn) is false, throw a TypeError exception.
5. Let newList be a new empty List.
6. Let k be 0.
7. Repeat, while k < len,
a. Let kValue be list[k].
b. Let mappedValue be ? Call(callbackfn, thisArg, « kValue, k, T »).
c. If Type(mappedValue) is Object, throw a TypeError exception.
d. Append mappedValue to the end of list newList.
e. Set k to k + 1.
8. Return a new Tuple value whose [[Sequence]] is newList.

*/

/* Step 1 */
/* map() should throw on a non-Tuple */
let method = Tuple.prototype.map;
let f = (x, i, tup) => x + 1;
assertEq(method.call(#[1,2,3,4,5,6],f), #[2,3,4,5,6,7]);
assertEq(method.call(Object(#[1,2,3,4,5,6]), f), #[2,3,4,5,6,7]);
assertThrowsInstanceOf(() => method.call("monkeys", f), TypeError,
                       "value of TupleObject must be a Tuple");

let tup = #[1,2,3];

/* Step 4 */
/* callbackfn not callable -- should throw */
assertThrowsInstanceOf(() => tup.map(), TypeError,
                       "missing function argument to Tuple.prototype.map");
assertThrowsInstanceOf(() => tup.map(undefined), TypeError,
                       "missing function argument to Tuple.prototype.map");
assertThrowsInstanceOf(() => tup.map("monkeys"), TypeError,
                       "bad function argument to Tuple.prototype.map");


/* callbackfn with 1 argument -- should be allowed */
var f2 = x => x * x;
assertEq(tup.map(f2), #[1, 4, 9]);

/* callbackfn with 2 arguments -- should be allowed */
f2 = (x, i) => x + i;
assertEq(tup.map(f2), #[1, 3, 5]);

/* callbackfn with > 3 arguments -- subsequent ones will be undefined */
var f3 = (a, b, c, d, e) => e === undefined;
assertEq(tup.map(f3), #[true, true, true]);

/* callbackfn should be able to use index and tuple */
var f4 = (x, i, tup) => (tup.indexOf(x+1) * i * x);
assertEq(tup.map(f4), #[0, 4, -6]);

/* explicit thisArg */
f1 = function (x, i, tup) { return(this.elements.indexOf(x) * x); };
assertEq(#[1,2,3,4,5].map(f1, { elements: [2, 4] }), #[-1, 0, -3, 4, -5]);

/* Step 3 */
/* Test that length is still handled correctly if it's overridden */
Object.defineProperty(Tuple.prototype, "length", { get() { return 0 } })
assertEq(tup.map(f), #[2,3,4]);

/* Step 7 */
assertEq(#[].map(f), #[]);
assertEq(#[1].map(f), #[2]);
assertEq(#[1,2].map(f), #[2,3]);

/* Step 7c. */
var badFun = x => new Object(x);
assertThrowsInstanceOf(() => tup.map(badFun),
                       TypeError, "Tuples cannot contain Objects");

reportCompare(0, 0);
