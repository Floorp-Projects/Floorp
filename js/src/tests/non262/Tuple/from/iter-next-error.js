// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/* Step 6b. */
/* AddEntriesFromIterable should throw if next() throws */

var iter = {};
iter[Symbol.iterator] = function() {
  return {
    next: function() {
      throw new SyntaxError();
    }
  };
};

assertThrowsInstanceOf(function() {
  Tuple.from(iter);
}, SyntaxError, "from() should throw if next() throws");

reportCompare(0, 0);
