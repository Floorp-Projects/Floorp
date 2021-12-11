
var x = {};
Object.defineProperty(x, 0, { configurable: true, value: null });
x.p = 0
Object.defineProperty(x, 0, { value: 2 });
