// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var array = [2, 4, 0, 16];
var expectedTuple = #[2, 4, undefined, 16];
var obj = {
  length: 4,
  0: 2,
  1: 4,
  2: 0,
  3: 16
};
delete obj[2];
let t = Tuple.from(obj);
assertEq(t, expectedTuple);

reportCompare(0, 0);
