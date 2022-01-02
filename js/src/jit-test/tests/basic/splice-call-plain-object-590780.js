var o = { 0: 1, 1: 2, 2: 3, length: 3 };
Array.prototype.splice.call(o, 0, 1);

assertEq(o[0], 2);
assertEq(o[1], 3);
assertEq(Object.getOwnPropertyDescriptor(o, 2), undefined);
assertEq("2" in o, false);
assertEq(o.length, 2);
