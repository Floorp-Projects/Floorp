// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var TuplePrototype = Tuple.prototype;

assertEq(typeof TuplePrototype.map, 'function');

assertThrowsInstanceOf(function() {
  TuplePrototype.map();
}, TypeError, "Tuple.prototype.map() with no argument");

reportCompare(0, 0);
