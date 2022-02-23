// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

assertEq(isConstructor(Tuple.of), false);

assertThrowsInstanceOf(() => {
  new Tuple.of(1, 2, 3);
}, TypeError, '`new Tuple.of(1, 2, 3)` throws TypeError');

reportCompare(0, 0);
