// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var desc = Object.getOwnPropertyDescriptor(Tuple.prototype.map, "length");
assertEq(desc.value, 1);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);


desc = Object.getOwnPropertyDescriptor(Tuple.prototype.map, "name");
assertEq(desc.value, "map");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

desc = Object.getOwnPropertyDescriptor(Tuple.prototype, "map");
assertEq(desc.writable, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(isConstructor(Tuple.prototype.map), false);

assertThrowsInstanceOf(() => {
  let t = #[1];
  new t.map();
}, TypeError, '`let t = #[1]; new t.map()` throws TypeError');

reportCompare(0, 0);
