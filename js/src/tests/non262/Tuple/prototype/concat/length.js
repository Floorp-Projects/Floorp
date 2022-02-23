// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var desc = Object.getOwnPropertyDescriptor(Tuple.prototype.concat, "length");
assertEq(desc.value, 0);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);


desc = Object.getOwnPropertyDescriptor(Tuple.prototype.concat, "name");
assertEq(desc.value, "concat");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

desc = Object.getOwnPropertyDescriptor(Tuple.prototype, "concat");
assertEq(desc.writable, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(isConstructor(Tuple.prototype.concat), false);

assertThrowsInstanceOf(() => {
  let t = #[1];
  new t.concat();
}, TypeError, '`let t = #[1]; new t.concat()` throws TypeError');

reportCompare(0, 0);
