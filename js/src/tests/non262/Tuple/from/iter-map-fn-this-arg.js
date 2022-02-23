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
var mapFn = function() {
  thisVals.push(this);
};
var items = {};
var thisVal = {};

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

Tuple.from(items, mapFn, thisVal);

assertEq(thisVals.length, 2);
assertEq(thisVals[0], thisVal);
assertEq(thisVals[1], thisVal);

reportCompare(0, 0);
