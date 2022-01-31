// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var desc = Object.getOwnPropertyDescriptor(Tuple.prototype.slice, "length");
assertEq(desc.value, 2);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

desc = Object.getOwnPropertyDescriptor(Tuple.prototype.slice, "name");
assertEq(desc.value, "slice");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

desc = Object.getOwnPropertyDescriptor(Tuple.prototype, "slice");
assertEq(desc.writable, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(isConstructor(Tuple.prototype.slice), false);

assertThrowsInstanceOf(() => {
  let t = #[1];
  new t.slice();
}, TypeError, '`let t = #[1]; new t.slice()` throws TypeError');

reportCompare(0, 0);
