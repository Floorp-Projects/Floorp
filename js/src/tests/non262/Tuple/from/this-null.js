// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

// this = null or undefined should not throw
var result = Tuple.from.call(null, #[]);

assertEq(result, #[]);
result = Tuple.from.call(undefined, #[]);
assertEq(result, #[]);
result = Tuple.from.call({}, #[]);
assertEq(result, #[]);
result = Tuple.from.call(5, #[]);
assertEq(result, #[]);

reportCompare(0, 0);
