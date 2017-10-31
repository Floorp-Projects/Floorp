assertEq(Object.keys(Promise).length, 0);
assertEq(Object.keys(Promise.prototype).length, 0);

reportCompare(0, 0, "ok");
