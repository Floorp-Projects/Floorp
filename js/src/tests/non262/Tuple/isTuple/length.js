// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

desc = Object.getOwnPropertyDescriptor(Tuple.isTuple, "length");
assertEq(desc.value, 1);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

reportCompare(0, 0);
