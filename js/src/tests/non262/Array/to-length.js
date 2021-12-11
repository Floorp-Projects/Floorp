const max = Number.MAX_SAFE_INTEGER;

assertEq(Array.prototype.indexOf.call({length: Infinity, [max - 1]: 'test'}, 'test', max - 3), max - 1);
assertEq(Array.prototype.lastIndexOf.call({length: Infinity, [max - 2]: 'test', [max - 1]: 'test2'}, 'test'), max - 2);

// ToLength doesn't truncate Infinity to zero, so the callback should be invoked
assertThrowsValue(() => Array.prototype.every.call({length: Infinity, [0]: undefined}, () => { throw "invoked" }), "invoked");
assertThrowsValue(() => Array.prototype.some.call({length: Infinity, [0]: undefined}, () => { throw "invoked" }), "invoked");
// Timeout before calling our callback
// assertThrowsValue(() => Array.prototype.sort.call({length: Infinity}, () => { throw "invoked" }), "invoked");
assertThrowsValue(() => Array.prototype.forEach.call({length: Infinity, [0]: undefined}, () => { throw "invoked" }), "invoked");
assertThrowsValue(() => Array.prototype.filter.call({length: Infinity, [0]: undefined}, () => { throw "invoked" }), "invoked");
assertThrowsValue(() => Array.prototype.reduce.call({length: Infinity, [0]: undefined, [1]: undefined}, () => { throw "invoked" }), "invoked");
assertThrowsValue(() => Array.prototype.reduceRight.call({length: Infinity, [max - 1]: undefined, [max - 2]: undefined}, () => { throw "invoked" }), "invoked");
assertThrowsValue(() => Array.prototype.find.call({length: Infinity}, () => { throw "invoked" }), "invoked");
assertThrowsValue(() => Array.prototype.findIndex.call({length: Infinity}, () => { throw "invoked" }), "invoked");
assertThrowsValue(() => Array.prototype.fill.call({length: Infinity, set "0"(v) { throw "invoked"; }}, () => { throw "invoked" }), "invoked");
assertThrowsValue(() => Array.prototype.copyWithin.call({length: Infinity, get [max - 2]() { throw "invoked"; }}, max - 2, max - 2), "invoked");

assertEq(Array.prototype.includes.call({length: Infinity, [max - 1]: "test"}, "test", max - 3), true);

// Invoking the Array constructor with MAX_SAFE_INTEGER will throw, 0 won't
assertThrowsInstanceOf(() => Array.from({length: Infinity}), RangeError);

// Make sure ArraySpeciesCreate is called with ToLength applied to the length property
var proxy = new Proxy([], {
    get(target, property) {
        if (property === "length")
            return Infinity;

        assertEq(property, "constructor");
        function fakeConstructor(length) { assertEq(length, max); throw "invoked"; };
        fakeConstructor[Symbol.species] = fakeConstructor;
        return fakeConstructor;
    }
})
assertThrowsValue(() => Array.prototype.map.call(proxy, () => { }), "invoked");

if (typeof reportCompare === "function")
    reportCompare(true, true);
