// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var array = [2, 4, 8, 16, 32, 64, 128];

function mapFnThrows(value, index, obj) {
  throw new RangeError();
}

assertThrowsInstanceOf(function() {
  Tuple.from(array, mapFnThrows);
}, RangeError, 'Tuple.from(array, mapFnThrows) throws');

reportCompare(0, 0);
