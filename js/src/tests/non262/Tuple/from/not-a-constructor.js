// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
assertEq(isConstructor(Tuple.from), false);

assertThrowsInstanceOf(() => {
  new Tuple.from([]);
}, TypeError, 'new Tuple.from([]) throws a TypeError exception');


reportCompare(0, 0);
