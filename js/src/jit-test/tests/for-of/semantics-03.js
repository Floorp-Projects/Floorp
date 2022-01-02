// Replacing Array.prototype.iterator with a generator affects for-of behavior.

load(libdir + "iteration.js");

Array.prototype[Symbol.iterator] = function* () {
    for (var i = this.length; --i >= 0; )
        yield this[i];
};

var s = '';
for (var v of ['a', 'b', 'c', 'd'])
    s += v;
assertEq(s, 'dcba');
