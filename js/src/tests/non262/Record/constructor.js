// |reftest| skip-if(!this.hasOwnProperty("Record"))

assertThrowsInstanceOf(
  () => new Record(),
  TypeError,
  "Record is not a constructor"
);

assertEq(typeof Record(), "record");
//assertEq(typeof Object(Record()), "record");
assertEq(Record() instanceof Record, false);
//assertThrowsInstanceOf(() => Object(Record()) instanceof Record, TypeError);

if (typeof reportCompare === "function") reportCompare(0, 0);
