// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

desc = Object.getOwnPropertyDescriptor(Tuple.from, "name");
assertEq(desc.value, "from");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

reportCompare(0, 0);
