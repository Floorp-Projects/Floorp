// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var desc = Object.getOwnPropertyDescriptor(Tuple.prototype.toSpliced, "length");
assertEq(desc.value, 2);
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);


desc = Object.getOwnPropertyDescriptor(Tuple.prototype.toSpliced, "name");
assertEq(desc.value, "toSpliced");
assertEq(desc.writable, false);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

desc = Object.getOwnPropertyDescriptor(Tuple.prototype, "toSpliced");
assertEq(desc.writable, true);
assertEq(desc.enumerable, false);
assertEq(desc.configurable, true);

assertEq(isConstructor(Tuple.prototype.toSpliced), false);

assertThrowsInstanceOf(() => {
  let t = #[1];
  new t.toSpliced();
}, TypeError, '`let t = #[1]; new t.toSpliced()` throws TypeError');

reportCompare(0, 0);
