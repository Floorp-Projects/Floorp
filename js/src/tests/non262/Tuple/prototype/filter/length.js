// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var desc = Object.getOwnPropertyDescriptor(Tuple.prototype.filter, "length");
assertEq(desc.value, 1);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);


desc = Object.getOwnPropertyDescriptor(Tuple.prototype.filter, "name");
assertEq(desc.value, "filter");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

desc = Object.getOwnPropertyDescriptor(Tuple.prototype, "filter");
assertEq(desc.writable, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(isConstructor(Tuple.prototype.filter), false);

assertThrowsInstanceOf(() => {
  let t = #[1];
  new t.filter();
}, TypeError, '`let t = #[1]; new t.filter()` throws TypeError');

reportCompare(0, 0);
