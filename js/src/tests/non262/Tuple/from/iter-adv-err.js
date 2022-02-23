// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var items = {};
items[Symbol.iterator] = function() {
  return {
    next: function() {
      throw new RangeError();
    }
  };
};

assertThrowsInstanceOf(function() {
  Tuple.from(items);
}, RangeError, 'Tuple.from(items) should throw');

reportCompare(0, 0);
