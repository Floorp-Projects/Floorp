// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var desc = Object.getOwnPropertyDescriptor(Tuple.prototype.indexOf, "length");
assertEq(desc.value, 1);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);


desc = Object.getOwnPropertyDescriptor(Tuple.prototype.indexOf, "name");
assertEq(desc.value, "indexOf");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

desc = Object.getOwnPropertyDescriptor(Tuple.prototype, "indexOf");
assertEq(desc.writable, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(isConstructor(Tuple.prototype.indexOf), false);

assertThrowsInstanceOf(() => {
  let t = #[1];
  new t.indexOf();
}, TypeError, '`let t = #[1]; new t.indexOf()` throws TypeError');

reportCompare(0, 0);
