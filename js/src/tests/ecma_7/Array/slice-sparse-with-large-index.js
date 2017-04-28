var array = [];
array[2**31 - 2] = "INT32_MAX - 1";
array[2**31 - 1] = "INT32_MAX";
array[2**31 - 0] = "INT32_MAX + 1";
array[2**32 - 2] = "UINT32_MAX - 1";
array[2**32 - 1] = "UINT32_MAX";
array[2**32 - 0] = "UINT32_MAX + 1";

var copy = array.slice();
assertEq(copy[2**31 - 2], "INT32_MAX - 1");
assertEq(copy[2**31 - 1], "INT32_MAX");
assertEq(copy[2**31 - 0], "INT32_MAX + 1");
assertEq(copy[2**32 - 2], "UINT32_MAX - 1");
assertEq(copy[2**32 - 1], undefined);
assertEq(copy[2**32 - 0], undefined);

if (typeof reportCompare === "function")
    reportCompare(true, true);
