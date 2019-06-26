// |jit-test| skip-if: !('oomTest' in this)
a = [];
minorgc();
Object.defineProperty(a, 12, {}).push(1);
toString = (function() { return a.reverse(); });
oomTest(Date.prototype.toJSON);
oomTest(Date.prototype.toJSON);
