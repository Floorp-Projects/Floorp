// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var arrayBuffer = new ArrayBuffer(7);

var result = Tuple.from(arrayBuffer);

assertEq(result.length, 0);

reportCompare(0, 0);
