// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

var tup = #[1, 2, 3];

var keys = Object.keys(tup);
assertEq(keys.length, 3);
assertEq(keys[0], "0");
assertEq(keys[1], "1");
assertEq(keys[2], "2");

var values = Object.values(tup);
assertEq(values.length, 3);
assertEq(values[0], 1);
assertEq(values[1], 2);
assertEq(values[2], 3);

var entries = Object.entries(tup);
assertEq(entries.length, 3);
assertEq(entries[0][0], "0");
assertEq(entries[0][1], 1);
assertEq(entries[1][0], "1");
assertEq(entries[1][1], 2);
assertEq(entries[2][0], "2");
assertEq(entries[2][1], 3);

var ownKeys = Reflect.ownKeys(Object(tup));
assertEq(ownKeys.length, 3);
assertEq(ownKeys[0], "0");
assertEq(ownKeys[1], "1");
assertEq(ownKeys[2], "2");

var spreadKeys = Object.keys({ ...tup });
assertEq(spreadKeys.length, 3);
assertEq(spreadKeys[0], "0");
assertEq(spreadKeys[1], "1");
assertEq(spreadKeys[2], "2");

var i = 0;
for (var key in tup) {
  switch (i++) {
	case 0: assertEq(key, "0"); break;
	case 1: assertEq(key, "1"); break;
	case 2: assertEq(key, "2"); break;
	default: assertUnreachable();
  }
}

if (typeof reportCompare === "function") reportCompare(0, 0);
