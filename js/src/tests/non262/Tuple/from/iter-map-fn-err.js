// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var closeCount = 0;
var mapFn = function() {
  throw new RangeError();
};
var items = {};
items[Symbol.iterator] = function() {
  return {
    return: function() {
      closeCount += 1;
    },
    next: function() {
      return {
        done: false
      };
    }
  };
};

assertThrowsInstanceOf(function() {
  Tuple.from(items, mapFn);
}, RangeError, 'Tuple.from(items, mapFn) should throw');

assertEq(closeCount, 1);

reportCompare(0, 0);
