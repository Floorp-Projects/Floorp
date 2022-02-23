// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.3.17 Tuple.prototype.flatMap ( mapperFunction [ , thisArg ] )
When the flatMap method is called with one or two arguments, the following steps are taken:

1. Let T be ? thisTupleValue(this value).
2. Let list be T.[[Sequence]].
3. If ! IsCallable(mapperFunction) is false, throw a TypeError exception.
4. Let flat be a new empty List.
5. Perform ? FlattenIntoTuple(flat, list, 1, mapperFunction, thisArg).
6. Return a new Tuple value whose [[Sequence]] is flat.

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
/* flatMap() should throw on a non-Tuple */
let method = Tuple.prototype.flatMap;
let id = x => x;
assertEq(method.call(#[1,#[2],3], id), #[1,2,3]);
assertEq(method.call(Object(#[1,#[2],3]), id), #[1,2,3]);
assertThrowsInstanceOf(() => method.call("monkeys", id), TypeError,
                       "value of TupleObject must be a Tuple");
assertThrowsInstanceOf(() => method.call(null, id), TypeError,
                       "value of TupleObject must be a Tuple");
assertThrowsInstanceOf(() => method.call(id), TypeError,
                       "value of TupleObject must be a Tuple");


let tup = #[1,2,#[3,#[4,5],6],#[5,6],7];
let tup2 = #[1, #[2], 3];

/* Step 4 */
/* callbackfn not callable -- should throw */
assertThrowsInstanceOf(() => tup.flatMap(), TypeError,
                       "missing function argument to Tuple.prototype.flatMap");
assertThrowsInstanceOf(() => tup.flatMap(undefined), TypeError,
                       "missing function argument to Tuple.prototype.flatMap");
assertThrowsInstanceOf(() => tup.flatMap("monkeys"), TypeError,
                       "bad function argument to Tuple.prototype.flatMap");


/* callbackfn with 1 argument -- should be allowed */
var f2 = function(x) {
    if (typeof(x) === "number") {
        return(x * x);
    } else {
        return 0;
    }
};
assertEq(tup2.flatMap(f2), #[1, 0, 9]);

/* callbackfn with 2 arguments -- should be allowed */
f2 = function(x, i) {
    if (typeof(x) === "number") {
        return(x + i);
    } else {
        return(i);
    }
};
assertEq(tup2.flatMap(f2), #[1, 1, 5]);

/* callbackfn with > 3 arguments -- subsequent ones will be undefined */
var f3 = (a, b, c, d, e) => e === undefined;
assertEq(tup2.flatMap(f3), #[true, true, true]);

/* callbackfn should be able to use index and tuple */
var f4 = function (x, i, tup) {
    if (typeof(x) === "number") {
        return(tup.indexOf(x+1) * i * x);
    } else {
        return(tup.indexOf(x) * i);
    }
}
assertEq(tup2.flatMap(f4), #[-0, 1, -6]);

/* explicit thisArg */
f1 = function (x, i, tup) {
    if (typeof(x) == "number") {
        return(this.elements.indexOf(x) * x);
    } else {
        return(this.elements.indexOf(x));
    }
}
assertEq(#[1,2,#[3,4],#[5]].flatMap(f1, { elements: [2, 4] }), #[-1, 0, -1, -1]);

/* FlattenIntoTuple steps */
/* Step 6.a.ii. */
var badF = x => new Object(x);
assertThrowsInstanceOf(() => tup.flatMap(badF), TypeError,
                       "Tuple cannot contain Object");
/* Step 6.b.i. */
var f = x => #[x, x];
assertEq(#[1,#[2,3],4].flatMap(f), #[1,1,#[2,3],#[2,3],4,4]);

reportCompare(0, 0);
