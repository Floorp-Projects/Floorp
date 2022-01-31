// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/* Step 6b. */
/* AddEntriesFromIterable should throw if next() throws */

var iter = {};
iter[Symbol.iterator] = function() {
  return {
    next: function() {
      var result = {};
      Object.defineProperty(result, 'value', {
        get: function() {
          throw new SyntaxError();
        }
      });

      return result;
    }
  };
};

assertThrowsInstanceOf(function() {
  Tuple.from(iter);
}, SyntaxError, "from() should throw if next() returns value that throws");

reportCompare(0, 0);
