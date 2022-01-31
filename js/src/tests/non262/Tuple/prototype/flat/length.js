// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var desc = Object.getOwnPropertyDescriptor(Tuple.prototype.flat, "length");
assertEq(desc.value, 0);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);


desc = Object.getOwnPropertyDescriptor(Tuple.prototype.flat, "name");
assertEq(desc.value, "flat");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

desc = Object.getOwnPropertyDescriptor(Tuple.prototype, "flat");
assertEq(desc.writable, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(isConstructor(Tuple.prototype.flat), false);

assertThrowsInstanceOf(() => {
  let t = #[1];
  new t.flat();
}, TypeError, '`let t = #[1]; new t.flat()` throws TypeError');

reportCompare(0, 0);
