// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var desc = Object.getOwnPropertyDescriptor(Tuple.from, "length");
assertEq(desc.value, 1);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(isConstructor(Tuple.from), false);

assertThrowsInstanceOf(() => {
  new Tuple.from([]);
}, TypeError, '`new Tuple.from()` throws TypeError');

reportCompare(0, 0);
