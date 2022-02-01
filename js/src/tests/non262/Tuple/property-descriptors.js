// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

var tup = Tuple(1, 2, 3);
var tupO = Object(tup);

var desc = Object.getOwnPropertyDescriptor(tupO, "0");
assertEq(desc.value, 1);
assertEq(desc.writable, false);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, false);

assertEq(Object.getOwnPropertyDescriptor(tupO, "3"), undefined);
assertEq(Object.getOwnPropertyDescriptor(tupO, "w"), undefined);
assertEq(Object.getOwnPropertyDescriptor(tupO, "length"), undefined);

assertThrowsInstanceOf(
  () => Object.defineProperty(tup, "0", { value: 1 }),
  TypeError,
  "#[1, 2, 3] is not a non-null object"
);

assertThrowsInstanceOf(
  () => Object.defineProperty(tupO, "b", {}),
  TypeError,
  'can\'t define property "b": tuple is not extensible'
);

assertThrowsInstanceOf(
  () => Object.defineProperty(tupO, "3", {}),
  TypeError,
  'can\'t define property "3": tuple is not extensible'
);

assertThrowsInstanceOf(
  () => Object.defineProperty(tupO, Symbol(), {}),
  TypeError,
  'can\'t define property "Symbol()": tuple is not extensible'
);

assertThrowsInstanceOf(
  () => Object.defineProperty(tupO, "0", { value: 2 }),
  TypeError,
  '"0" is read-only'
);

Object.defineProperty(tupO, "0", { value: 1 });

assertThrowsInstanceOf(
  () => Object.defineProperty(tupO, "0", { value: 1, writable: true }),
  TypeError,
  "Invalid tuple property descriptor"
);

assertThrowsInstanceOf(
  () => Object.defineProperty(tupO, "0", { value: 1, enumerable: false }),
  TypeError,
  "Invalid tuple property descriptor"
);

assertThrowsInstanceOf(
  () => Object.defineProperty(tupO, "0", { value: 1, configurable: true }),
  TypeError,
  "Invalid tuple property descriptor"
);

assertEq(Object.prototype.propertyIsEnumerable.call(tupO, "0"), true);
assertEq(Object.prototype.propertyIsEnumerable.call(tupO, "0"), true);
assertEq(Object.prototype.propertyIsEnumerable.call(tupO, "w"), false);
assertEq(Object.prototype.propertyIsEnumerable.call(tupO, "w"), false);
assertEq(Object.prototype.propertyIsEnumerable.call(tupO, "3"), false);
assertEq(Object.prototype.propertyIsEnumerable.call(tupO, "3"), false);

assertEq(Object.prototype.hasOwnProperty.call(tupO, "0"), true);
assertEq(Object.prototype.hasOwnProperty.call(tupO, "0"), true);
assertEq(Object.prototype.hasOwnProperty.call(tupO, "w"), false);
assertEq(Object.prototype.hasOwnProperty.call(tupO, "w"), false);
assertEq(Object.prototype.hasOwnProperty.call(tupO, "3"), false);
assertEq(Object.prototype.hasOwnProperty.call(tupO, "3"), false);

assertEq("0" in tupO, true);
assertEq("w" in tupO, false);
assertEq("3" in tupO, false);

assertEq(delete tupO[0], false);
assertEq(delete tupO[0], false);
assertEq(delete tupO.w, true);
assertEq(delete tupO.w, true);
assertEq(delete tupO[3], true);
assertEq(delete tupO[3], true);

if (typeof reportCompare === "function") reportCompare(0, 0);
