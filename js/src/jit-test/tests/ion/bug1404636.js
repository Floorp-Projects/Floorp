x = new Uint32Array(4);
try {
    Math.max(Uint32Array.prototype)();
} catch (e) {}
x[3] = -1;
assertEq(x.toString(), "0,0,0,4294967295");
