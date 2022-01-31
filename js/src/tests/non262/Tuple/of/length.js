// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var desc = Object.getOwnPropertyDescriptor(Tuple.of, "length");
assertEq(desc.value, 0);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

desc = Object.getOwnPropertyDescriptor(Tuple.of, "name");
assertEq(desc.value, "of");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(isConstructor(Tuple.of), false);

assertThrowsInstanceOf(() => {
  new Tuple.of(1, 2, 3);
}, TypeError, '`new Tuple.of(1, 2, 3)` throws TypeError');

reportCompare(0, 0);
