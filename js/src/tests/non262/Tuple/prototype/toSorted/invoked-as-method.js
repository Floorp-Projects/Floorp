// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var TuplePrototype = Tuple.prototype;

assertEq(typeof TuplePrototype.toSorted, 'function');

assertThrowsInstanceOf(function() {
  TuplePrototype.toSorted();
}, TypeError, "toSorted() invoked as method");

reportCompare(0, 0);
