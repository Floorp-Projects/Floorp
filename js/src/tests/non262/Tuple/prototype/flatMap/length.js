// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var desc = Object.getOwnPropertyDescriptor(Tuple.prototype.flatMap, "length");
assertEq(desc.value, 1);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);


desc = Object.getOwnPropertyDescriptor(Tuple.prototype.flatMap, "name");
assertEq(desc.value, "flatMap");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

desc = Object.getOwnPropertyDescriptor(Tuple.prototype, "flatMap");
assertEq(desc.writable, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(isConstructor(Tuple.prototype.flatMap), false);

assertThrowsInstanceOf(() => {
  let t = #[1];
  new t.flatMap();
}, TypeError, '`let t = #[1]; new t.flatMap()` throws TypeError');

reportCompare(0, 0);
