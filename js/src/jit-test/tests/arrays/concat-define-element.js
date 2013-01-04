var bigarr = [];
bigarr[5000] = 17;
Object.defineProperty(Object.prototype, 5000, { set: function() { throw 42; } });
assertEq([].concat(bigarr).length, 5001);
