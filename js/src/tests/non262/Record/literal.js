// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

let rec = #{ x: 1, "y": 2, 0: 3, 1n: 4, [`key${4}`]: 5 };

assertEq(rec.x, 1);
assertEq(rec.y, 2);
assertEq(rec[0], 3);
assertEq(rec[1], 4);
assertEq(rec.key4, 5);

// TODO: Handle duplicated keys
// let dup = #{ x: 1, x: 2 };
// assertEq(dup.x, 2);

assertThrowsInstanceOf(
	() => #{ [Symbol()]: 1 },
	TypeError,
	"Symbols cannot be used as record keys"
  );

if (typeof reportCompare === "function") reportCompare(0, 0);
