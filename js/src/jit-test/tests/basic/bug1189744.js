var obj;
for (var i = 0; i < 100; i++)
    obj = {a: 7, b: 13, c: 42, d: 0};

Object.defineProperty(obj, "x", {
    get: function () { return 3; }
});
obj.__ob__ = 17;

Object.defineProperty(obj, "c", {value: 8, writable: true});
assertEq(obj.__ob__, 17);
