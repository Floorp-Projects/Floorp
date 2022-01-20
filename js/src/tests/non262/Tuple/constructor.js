// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

assertThrowsInstanceOf(
  () => new Tuple(),
  TypeError,
  "Tuple is not a constructor"
);

assertEq(typeof Tuple(), "tuple");
assertEq(typeof Object(Tuple()), "object");
assertEq(Tuple() instanceof Tuple, false);
assertEq(Object(Tuple()) instanceof Tuple, true);

assertEq(Tuple().__proto__, Tuple.prototype);
assertEq(Object(Tuple()).__proto__, Tuple.prototype);
assertEq(Tuple.prototype.constructor, Tuple);

if (typeof reportCompare === "function") reportCompare(0, 0);
