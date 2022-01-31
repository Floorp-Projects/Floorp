// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var desc = Object.getOwnPropertyDescriptor(Tuple.isTuple, "length");
assertEq(desc.value, 1);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);


desc = Object.getOwnPropertyDescriptor(Tuple.isTuple, "name");
assertEq(desc.value, "isTuple");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(isConstructor(Tuple.isTuple), false);

assertThrowsInstanceOf(() => {
  let t = #[1];
    new t.isTuple()();
}, TypeError, '`let t = #[1]; new t.isTuple()` throws TypeError');

reportCompare(0, 0);
