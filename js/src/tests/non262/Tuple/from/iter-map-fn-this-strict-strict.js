// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
'use strict';
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

assertEq(thisVals.length, 2);
assertEq(thisVals[0], undefined);
assertEq(thisVals[1], undefined);

reportCompare(0, 0);
