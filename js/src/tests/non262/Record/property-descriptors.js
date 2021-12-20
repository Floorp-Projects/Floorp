// |reftest| skip-if(!this.hasOwnProperty("Record"))

var rec = Record({ x: 1, y: 2, a: 3 });

var desc = Object.getOwnPropertyDescriptor(rec, "x");
assertEq(desc.value, 1);
assertEq(desc.writable, false);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, false);

assertEq(Object.getOwnPropertyDescriptor(rec, "z"), undefined);

var recO = Object(rec);

assertThrowsInstanceOf(
	() => Object.defineProperty(rec, "x", { value: 1 }),
	TypeError,
	"#{ \"a\": 3, \"x\": 1, \"y\": 2 } is not a non-null object"
);

assertThrowsInstanceOf(
	() => Object.defineProperty(recO, "b", {}),
	TypeError,
	"can't define property \"b\": Record is not extensible"
);

assertThrowsInstanceOf(
	() => Object.defineProperty(recO, Symbol(), {}),
	TypeError,
	"can't define property \"b\": Record is not extensible"
);

assertThrowsInstanceOf(
	() => Object.defineProperty(recO, "x", { value: 2 }),
	TypeError,
	'"x" is read-only'
);

Object.defineProperty(recO, "x", { value: 1 });

assertThrowsInstanceOf(
	() => Object.defineProperty(recO, "x", { value: 2, writable: true }),
	TypeError,
	'Invalid record property descriptor'
);

assertThrowsInstanceOf(
	() => Object.defineProperty(recO, "x", { value: 2, enumerable: false }),
	TypeError,
	'Invalid record property descriptor'
);

assertThrowsInstanceOf(
	() => Object.defineProperty(recO, "x", { value: 2, configurable: true }),
	TypeError,
	'Invalid record property descriptor'
);

assertEq(Object.prototype.propertyIsEnumerable.call(rec, "x"), true);
assertEq(Object.prototype.propertyIsEnumerable.call(rec, "x"), true);
assertEq(Object.prototype.propertyIsEnumerable.call(rec, "w"), false);
assertEq(Object.prototype.propertyIsEnumerable.call(rec, "w"), false);
assertEq(Object.prototype.propertyIsEnumerable.call(recO, "x"), true);
assertEq(Object.prototype.propertyIsEnumerable.call(recO, "x"), true);
assertEq(Object.prototype.propertyIsEnumerable.call(recO, "w"), false);
assertEq(Object.prototype.propertyIsEnumerable.call(recO, "w"), false);

assertEq(Object.prototype.hasOwnProperty.call(rec, "x"), true);
assertEq(Object.prototype.hasOwnProperty.call(rec, "x"), true);
assertEq(Object.prototype.hasOwnProperty.call(rec, "w"), false);
assertEq(Object.prototype.hasOwnProperty.call(rec, "w"), false);
assertEq(Object.prototype.hasOwnProperty.call(recO, "x"), true);
assertEq(Object.prototype.hasOwnProperty.call(recO, "x"), true);
assertEq(Object.prototype.hasOwnProperty.call(recO, "w"), false);
assertEq(Object.prototype.hasOwnProperty.call(recO, "w"), false);

assertEq("x" in recO, true);
assertEq("w" in recO, false);

assertEq(delete rec.x, false);
assertEq(delete rec.x, false);
assertEq(delete rec.w, true);
assertEq(delete rec.w, true);
assertEq(delete recO.x, false);
assertEq(delete recO.x, false);
assertEq(delete recO.w, true);
assertEq(delete recO.w, true);

if (typeof reportCompare === "function") reportCompare(0, 0);
