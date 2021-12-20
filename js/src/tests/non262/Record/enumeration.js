// |reftest| skip-if(!this.hasOwnProperty("Record"))

var rec = #{ x: 1, y: 2, a: 3 };

var keys = Object.keys(rec);
assertEq(keys.length, 3);
assertEq(keys[0], "a");
assertEq(keys[1], "x");
assertEq(keys[2], "y");

var values = Object.values(rec);
assertEq(values.length, 3);
assertEq(values[0], 3);
assertEq(values[1], 1);
assertEq(values[2], 2);

var entries = Object.entries(rec);
assertEq(entries.length, 3);
assertEq(entries[0][0], "a");
assertEq(entries[0][1], 3);
assertEq(entries[1][0], "x");
assertEq(entries[1][1], 1);
assertEq(entries[2][0], "y");
assertEq(entries[2][1], 2);

var ownKeys = Reflect.ownKeys(Object(rec));
assertEq(ownKeys.length, 3);
assertEq(ownKeys[0], "a");
assertEq(ownKeys[1], "x");
assertEq(ownKeys[2], "y");

var spreadKeys = Object.keys({ ...rec });
assertEq(spreadKeys.length, 3);
assertEq(spreadKeys[0], "a");
assertEq(spreadKeys[1], "x");
assertEq(spreadKeys[2], "y");

var spreadKeysObj = Object.keys({ ...Object(rec) });
assertEq(spreadKeysObj.length, 3);
assertEq(spreadKeysObj[0], "a");
assertEq(spreadKeysObj[1], "x");
assertEq(spreadKeysObj[2], "y");

var i = 0;
for (var key in rec) {
  switch (i++) {
	case 0: assertEq(key, "a"); break;
	case 1: assertEq(key, "x"); break;
	case 2: assertEq(key, "y"); break;
	default: assertUnreachable();
  }
}

if (typeof reportCompare === "function") reportCompare(0, 0);
