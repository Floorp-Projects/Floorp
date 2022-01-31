// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

desc = Object.getOwnPropertyDescriptor(Tuple.valueOf, "name");
assertEq(desc.value, "valueOf");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

reportCompare(0, 0);
