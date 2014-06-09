load(libdir + "asserts.js");
load(libdir + "iteration.js");
load(libdir + "eqArrayHelper.js");

assertEqArray([...[1, 2, 3]], [1, 2, 3]);
assertEqArray([1, ...[2, 3, 4], 5], [1, 2, 3, 4, 5]);
assertEqArray([1, ...[], 2], [1, 2]);
assertEqArray([1, ...[2, 3], 4, ...[5, 6]], [1, 2, 3, 4, 5, 6]);
assertEqArray([1, ...[], 2], [1, 2]);
assertEqArray([1,, ...[2]], [1,, 2]);
assertEqArray([1,, ...[2],, 3,, 4,], [1,, 2,, 3,, 4,]);
assertEqArray([...[1, 2, 3],,,,], [1, 2, 3,,,,]);
assertEqArray([,,...[1, 2, 3],,,,], [,,1,2,3,,,,]);
assertEqArray([...[1, 2, 3],,,,...[]], [1,2,3,,,,]);

assertEqArray([...[undefined]], [undefined]);

// other iterable objects
assertEqArray([...new Int32Array([1, 2, 3])], [1, 2, 3]);
assertEqArray([..."abc"], ["a", "b", "c"]);
assertEqArray([...[1, 2, 3][std_iterator]()], [1, 2, 3]);
assertEqArray([...Set([1, 2, 3])], [1, 2, 3]);
assertEqArray([...Map([["a", "A"], ["b", "B"], ["c", "C"]])].map(([k, v]) => k + v), ["aA", "bB", "cC"]);
let itr = {};
itr[std_iterator] = function () {
    return {
        i: 1,
        next: function() {
            if (this.i < 4)
                return { value: this.i++, done: false };
            else
                return { value: undefined, done: true };
        }
    };
}
assertEqArray([...itr], [1, 2, 3]);
function* gen() {
    for (let i = 1; i < 4; i ++)
        yield i;
}
assertEqArray([...gen()], [1, 2, 3]);

let a, b = [1, 2, 3];
assertEqArray([...a=b], [1, 2, 3]);

// 12.2.4.1.2 Runtime Semantics: ArrayAccumulation
// If Type(spreadObj) is not Object, then throw a TypeError exception.
assertThrowsInstanceOf(() => [...null], TypeError);
assertThrowsInstanceOf(() => [...undefined], TypeError);

