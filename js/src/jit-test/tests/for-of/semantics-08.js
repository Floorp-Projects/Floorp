// A for-of loop exits if the iterator's .next method throws another compartment's StopIteration.

var g = newGlobal();
var it = g.eval("({ iterator: function () { return this; }, " +
                "next: function () { throw StopIteration; } });");
for (x of it)
    throw 'FAIL';
