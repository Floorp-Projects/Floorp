// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

var desc = Object.getOwnPropertyDescriptor(Tuple.prototype, Symbol.toStringTag);

assertEq(desc.length, undefined);

assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

reportCompare(0, 0);
