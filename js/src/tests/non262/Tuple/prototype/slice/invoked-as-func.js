// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var slice = Tuple.prototype.slice;

assertThrowsInstanceOf(function() { slice() }, TypeError,
                       "value of TupleObject must be a Tuple");

reportCompare(0, 0);
