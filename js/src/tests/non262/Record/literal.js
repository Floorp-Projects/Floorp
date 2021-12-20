// |reftest| skip-if(!this.hasOwnProperty("Record"))

let rec = #{ x: 1, "y": 2, 0: 3, 1n: 4, [`key${4}`]: 5 };

assertEq(rec.x, 1);
assertEq(rec.y, 2);
assertEq(rec[0], 3);
assertEq(rec[1], 4);
assertEq(rec.key4, 5);

let dup = #{ x: 1, x: 2 };
assertEq(dup.x, 2);

assertThrowsInstanceOf(
	() => #{ [Symbol()]: 1 },
	TypeError,
	"Symbols cannot be used as record keys"
  );

let rec2 = #{ x: 1, ...{ a: 2, z: 3 }, b: 4, ...{ d: 5 } };
assertEq(rec2.x, 1);
assertEq(rec2.a, 2);
assertEq(rec2.z, 3);
assertEq(rec2.b, 4);
assertEq(rec2.d, 5);

assertThrowsInstanceOf(
	() => #{ ...{ [Symbol()]: 1 } },
	TypeError,
	"Symbols cannot be used as record keys"
);

let rec3 = #{
	...Object.defineProperty({}, "x", { value: 1 }),
	...Object.defineProperty({}, Symbol(), { value: 2 }),
};
assertEq(rec3.x, undefined);

let rec4 = #{ ...{}, ...{}, ...{} };

if (typeof reportCompare === "function") reportCompare(0, 0);
