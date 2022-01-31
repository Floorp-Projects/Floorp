// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var toReversed = Tuple.prototype.toReversed;

assertEq(typeof toReversed, 'function');

assertThrowsInstanceOf(function() {
  toReversed();
}, TypeError, "toReversed() invoked as function");

reportCompare(0, 0);
