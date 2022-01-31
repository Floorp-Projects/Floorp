// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

var desc = Object.getOwnPropertyDescriptor(Tuple.prototype, Symbol.toStringTag);

assertEq(desc.name, undefined);

reportCompare(0, 0);
