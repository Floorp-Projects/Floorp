// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var TuplePrototype = Tuple.prototype;

assertEq(typeof TuplePrototype.slice, 'function');

assertThrowsInstanceOf(function() { TuplePrototype.slice() }, TypeError,
                       "value of TupleObject must be a Tuple");

reportCompare(0, 0);
