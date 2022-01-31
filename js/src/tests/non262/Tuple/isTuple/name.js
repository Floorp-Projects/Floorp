// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

desc = Object.getOwnPropertyDescriptor(Tuple.isTuple, "name");
assertEq(desc.value, "isTuple");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

reportCompare(0, 0);
