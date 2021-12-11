// Set getters should have get prefix
assertEq(Object.getOwnPropertyDescriptor(Set, Symbol.species).get.name, "get [Symbol.species]");
assertEq(Object.getOwnPropertyDescriptor(Set.prototype, "size").get.name, "get size");
