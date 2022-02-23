// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var map = Tuple.prototype.map;

assertEq(typeof map, 'function');

reportCompare(0, 0);
