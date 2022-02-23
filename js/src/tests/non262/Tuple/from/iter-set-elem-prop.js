// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var items = {};
var firstIterResult = {
  done: false,
  value: 1
};
var secondIterResult = {
  done: false,
  value: 2
};
var thirdIterResult = {
  done: true,
  value: 3
};
var nextIterResult = firstIterResult;
var nextNextIterResult = secondIterResult;
var result;

items[Symbol.iterator] = function() {
  return {
    next: function() {
      var result = nextIterResult;

      nextIterResult = nextNextIterResult;
      nextNextIterResult = thirdIterResult;

      return result;
    }
  };
};

result = Tuple.from(items);

assertEq(result[0], firstIterResult.value);
assertEq(result[1], secondIterResult.value);

reportCompare(0, 0);
