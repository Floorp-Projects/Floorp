var obj = new Proxy(Object.create(null), {});
assertEq(typeof obj, 'object');
assertEq(obj != null, true);
