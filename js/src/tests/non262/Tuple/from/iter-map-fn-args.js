// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var args = [];
var firstResult = {
  done: false,
  value: {}
};
var secondResult = {
  done: false,
  value: {}
};
var mapFn = function(value, idx) {
  args.push(arguments);
};
var items = {};
var nextResult = firstResult;
var nextNextResult = secondResult;

items[Symbol.iterator] = function() {
  return {
    next: function() {
      var result = nextResult;
      nextResult = nextNextResult;
      nextNextResult = {
        done: true
      };

      return result;
    }
  };
};

Tuple.from(items, mapFn);

assertEq(args.length, 2);

assertEq(args[0].length, 2);
assertEq(args[0][0], firstResult.value);
assertEq(args[0][1], 0);

assertEq(args[1].length, 2);
assertEq(args[1][0], secondResult.value);
assertEq(args[1][1], 1);

reportCompare(0, 0);
