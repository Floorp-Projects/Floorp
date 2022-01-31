// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

var desc = Object.getOwnPropertyDescriptor(Tuple.valueOf, "length");
assertEq(desc.value, 0);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(isConstructor(Tuple.valueOf), false);

assertThrowsInstanceOf(() => {
  new Tuple.valueOf([]);
}, TypeError, '`new Tuple.valueOf()` throws TypeError');

reportCompare(0, 0);
