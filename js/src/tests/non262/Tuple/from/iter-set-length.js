// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var items = {};
var result, nextIterResult, lastIterResult;
items[Symbol.iterator] = function() {
  return {
    next: function() {
      var result = nextIterResult;
      nextIterResult = lastIterResult;
      return result;
    }
  };
};

nextIterResult = lastIterResult = {
  done: true
};
result = Tuple.from(items);

assertEq(result.length, 0);

nextIterResult = {
  done: false
};
lastIterResult = {
  done: true
};
result = Tuple.from(items);

assertEq(result.length, 1);

reportCompare(0, 0);
