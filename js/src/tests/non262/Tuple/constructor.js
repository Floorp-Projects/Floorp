// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

assertThrowsInstanceOf(
  () => new Tuple(),
  TypeError,
  "Tuple is not a constructor"
);

assertEq(typeof Tuple(), "tuple");
//assertEq(typeof Object(Tuple()), "tuple");
assertEq(Tuple() instanceof Tuple, false);
//assertEq(Object(Tuple()) instanceof Tuple, true);

if (typeof reportCompare === "function") reportCompare(0, 0);
