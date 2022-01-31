// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.2.2 Tuple.from ( items [ , mapFn [, thisArg ] ] )
When the from method is called with argument items and optional arguments mapfn and thisArg, the following steps are taken:

1. If mapfn is undefined, let mapping be false.
2. Else,
a. If IsCallable(mapfn) is false, throw a TypeError exception.
b. Let mapping be true.
3. Let list be a new empty List.
4. Let k be 0.
5. Let usingIterator be ? GetMethod(items, @@iterator).
6. If usingIterator is not undefined, then
a. Let adder be a new Abstract Closure with parameters (key, value) that captures (list, mapFn, thisArg, mapping, k) and performs the following steps when called:
i. If mapping is true, then
1. Let mappedValue be ? Call(mapfn, thisArg, « value, k »).
ii. Else, let mappedValue be value.
iii. If Type(mappedValue) is Object, throw a TypeError exception.
iv. Append mappedValue to list.
v. Set k to k + 1.
b. Perform ! AddEntriesFromIterable(undefined, iterable, adder).
c. Return a new Tuple value whose [[Sequence]] is list.
7. NOTE: items is not an Iterable so assume it is an array-like object.
8. Let arrayLike be ! ToObject(items).
9. Let len be ? LengthOfArrayLike(arrayLike).
10. Repeat, while k < len,
a. Let Pk be ! ToString(k).
b. Let kValue be ? Get(arrayLike, Pk).
c. If mapping is true, then
i. Let mappedValue be ? Call(mapfn, thisArg, « kValue, k »).
d. Else, let mappedValue be kValue.
e. If Type(mappedValue) is Object, throw a TypeError exception.
f. Append mappedValue to list.
g. Set k to k + 1.
11. Return a new Tuple value whose [[Sequence]] is list.
*/

/* Step 1: mapfn explicitly undefined */
assertEq(Tuple.from([1,2],undefined),#[1,2]);

/* Step 2a: If IsCallable(mapfn) is false, throw a TypeError exception. */
assertThrowsInstanceOf(() => Tuple.from([1,2,3],"monkeys"), TypeError, "mapfn not callable");

/* Step 5 */
assertThrowsInstanceOf(() => Tuple.from(undefined), TypeError,
                       "can't access property Symbol.iterator of undefined");
assertThrowsInstanceOf(() => Tuple.from(null), TypeError,
                       "can't access property Symbol.iterator of null");
assertEq(Tuple.from(1), #[]);
var obj = {};
obj[Symbol.iterator] = null;
assertEq(Tuple.from(obj), #[]);
obj[Symbol.iterator] = 5;
assertThrowsInstanceOf(() => Tuple.from(obj), TypeError,
                       "number is not a function");


/* Step 6a.i. mapping exists */
assertEq(Tuple.from([1,2,3],x => x+1), #[2,3,4]);
assertEq(Tuple.from(#[1,2,3],x => x+1), #[2,3,4]);
assertEq(Tuple.from("xyz", c => String.fromCharCode(c.charCodeAt(0) + 1)), #['y', 'z', '{']);
/* Step 6a.ii. mapping does not exist */
assertEq(Tuple.from([1,2,3,4,5]), #[1,2,3,4,5]);
assertEq(Tuple.from(#[1,2,3,4,5]), #[1,2,3,4,5]);
assertEq(Tuple.from("xyz"), #['x', 'y', 'z']);

/* Step 6a.iii. if mapfn returns Object, throw */
assertThrowsInstanceOf(() => Tuple.from([1,2], x => [x]), TypeError, "Tuple cannot contain object");

/* Step 7 -- array-like but not iterable */
var obj = { length: 3, 0: "a", 1: "b", 2: "c" };
assertEq(Tuple.from(obj), #["a","b","c"]);
assertEq(Tuple.from(obj, s => s + s), #["aa","bb","cc"]);

obj = { 0: "a", 1: "b", 2: "c" };
assertEq(Tuple.from(obj), #[]);

obj = { length: 3, 0: 1, 2: 3 };
assertEq(Tuple.from(obj), #[1, undefined, 3]);

obj = { length: 3, 0: 1, 1: [2], 2: 3 };
assertThrowsInstanceOf(() => Tuple.from(obj), TypeError, "Tuple cannot contain object");

/* Step 10e */
assertThrowsInstanceOf(() => Tuple.from([1, 2, [3,4]]), TypeError,
                       "Tuple can't contain Object");


reportCompare(0, 0);
