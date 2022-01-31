// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var itemsPoisonedSymbolIterator = {};
itemsPoisonedSymbolIterator[Symbol.iterator] = function() {
  throw new RangeError();
};

assertThrowsInstanceOf(function() {
  Tuple.from(itemsPoisonedSymbolIterator);
}, RangeError, 'Tuple.from(itemsPoisonedSymbolIterator) throws');

reportCompare(0, 0);
