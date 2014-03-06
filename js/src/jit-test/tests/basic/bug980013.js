
x = new Uint8ClampedArray;
x.__proto__ = {};
Object.defineProperty(this, "y", {
    get: function() {
        return x.length;
    }
});
assertEq(y, undefined);
