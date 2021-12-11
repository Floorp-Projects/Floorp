// new Map(arr) throws if arr contains holes (or undefined values).

load(libdir + "asserts.js");
assertThrowsInstanceOf(function () { new Map([undefined]); }, TypeError);
assertThrowsInstanceOf(function () { new Map([null]); }, TypeError);
assertThrowsInstanceOf(function () { new Map([[0, 0], [1, 1], , [3, 3]]); }, TypeError);
assertThrowsInstanceOf(function () { new Map([[0, 0], [1, 1], ,]); }, TypeError);

// new Map(iterable) throws if iterable doesn't have array-like objects

assertThrowsInstanceOf(function () { new Map([1, 2, 3]); }, TypeError);
assertThrowsInstanceOf(function () {
	let s = new Set([1, 2, "abc"]);
	new Map(s);
}, TypeError);
