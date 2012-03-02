// Map(arr) throws if arr contains holes (or undefined values).

load(libdir + "asserts.js");
assertThrowsInstanceOf(function () { Map([undefined]); }, TypeError);
assertThrowsInstanceOf(function () { Map([null]); }, TypeError);
assertThrowsInstanceOf(function () { Map([[0, 0], [1, 1], , [3, 3]]); }, TypeError);
assertThrowsInstanceOf(function () { Map([[0, 0], [1, 1], ,]); }, TypeError);
