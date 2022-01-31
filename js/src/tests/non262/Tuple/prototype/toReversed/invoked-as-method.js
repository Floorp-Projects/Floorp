// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var TuplePrototype = Tuple.prototype;

assertEq(typeof TuplePrototype.toReversed, 'function');

assertThrowsInstanceOf(function() {
  TuplePrototype.toReversed();
}, TypeError, "toReversed() invoked as method");

reportCompare(0, 0);
