// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var thisVals = [];
var nextResult = {
  done: false,
  value: {}
};
var nextNextResult = {
  done: false,
  value: {}
};
var firstReturnVal = 1;
var secondReturnVal = 2;
var mapFn = function(value, idx) {
  var returnVal = nextReturnVal;
  nextReturnVal = nextNextReturnVal;
  nextNextReturnVal = null;
  return returnVal;
};
var nextReturnVal = firstReturnVal;
var nextNextReturnVal = secondReturnVal;
var items = {};
var result;

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

result = Tuple.from(items, mapFn);

assertEq(result.length, 2);
assertEq(result[0], firstReturnVal);
assertEq(result[1], secondReturnVal);

reportCompare(0, 0);
