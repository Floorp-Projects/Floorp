// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var itemsPoisonedIteratorValue = {};
var poisonedValue = {};
Object.defineProperty(poisonedValue, 'value', {
  get: function() {
    throw new RangeError();
  }
});
itemsPoisonedIteratorValue[Symbol.iterator] = function() {
  return {
    next: function() {
      return poisonedValue;
    }
  };
};

assertThrowsInstanceOf(function() {
  Tuple.from(itemsPoisonedIteratorValue);
}, RangeError, 'Tuple.from(itemsPoisonedIteratorValue) throws');

reportCompare(0, 0);
