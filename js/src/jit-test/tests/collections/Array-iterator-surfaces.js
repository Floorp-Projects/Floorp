// Array.prototype.iterator is the same function object as .values.

assertEq(Array.prototype.values, Array.prototype.iterator);
assertEq(Array.prototype.iterator.name, "values");
