// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

var desc = Object.getOwnPropertyDescriptor(Tuple.prototype, "slice");
assertEq(desc.writable, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);
assertEq(typeof Tuple.prototype.slice, 'function');

reportCompare(0, 0);
