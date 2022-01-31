// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/* Use internal length rather than getting a length property */
var getCalls = 0;
var desc = {
  get: function getLen() {
    getCalls++;
    return 0;
  }
};

var internalLength = Object.getOwnPropertyDescriptor(Tuple.prototype, "length").get;
Object.defineProperty(Tuple.prototype, "length", desc);

let sample = #[42,42,42];

getCalls = 0;

var result = sample.toSorted();

assertEq(getCalls, 0)
assertEq(result, sample);
assertEq(result[0], sample[0]);
assertEq(result[1], sample[1]);
assertEq(result[2], sample[2]);
assertEq(result.length, 0);
assertEq(internalLength.call(result), 3);

reportCompare(0, 0);
