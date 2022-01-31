// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var toSorted = Tuple.prototype.toSorted;

assertEq(typeof toSorted, 'function');

assertThrowsInstanceOf(function() {
  toSorted();
}, TypeError, "toSorted invoked as function");

reportCompare(0, 0);
