// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var items = {};
Object.defineProperty(items, Symbol.iterator, {
  get: function() {
    throw new RangeError();
  }
});

assertThrowsInstanceOf(() => Tuple.from(items), RangeError,
                       'Tuple.from(items) should throw');

reportCompare(0, 0);
