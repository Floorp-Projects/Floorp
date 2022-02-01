// |reftest| skip-if(!this.hasOwnProperty("Record"))

assertThrowsInstanceOf(
  () => new Record(),
  TypeError,
  "Record is not a constructor"
);

assertEq(typeof Record({}), "record");
assertEq(typeof Object(Record({})), "object");
assertEq(Record({}) instanceof Record, false);

// TODO: It's still not decided what the prototype of records should be
//assertThrowsInstanceOf(() => Object(Record({})) instanceof Record, TypeError);
// assertEq(Record.prototype, null);
// assertEq(Record({}).__proto__, null);
// assertEq(Object(Record({})).__proto__, null);

assertThrowsInstanceOf(
  () => Record(),
  TypeError,
  "can't convert undefined to object"
);

assertThrowsInstanceOf(
  () => Record(undefined),
  TypeError,
  "can't convert undefined to object"
);

assertThrowsInstanceOf(
  () => Record(null),
  TypeError,
  "can't convert null to object"
);

if (typeof reportCompare === "function") reportCompare(0, 0);
