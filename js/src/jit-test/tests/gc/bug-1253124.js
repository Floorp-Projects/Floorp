if (!('oomTest' in this))
    quit();

for (let i = 0; i < 10; i++)
    toPrimitive = Date.prototype[Symbol.toPrimitive];
assertThrowsInstanceOf(() =>  0);
obj = {};
oomTest(() => assertThrowsInstanceOf(() => toPrimitive.call(obj, "boolean")));
function assertThrowsInstanceOf(f) {
    f();
}
