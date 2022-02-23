// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

/* Ensure that slice uses internal length and not length property */


var getCalls = 0;
var desc = {
  get: function getLen() {
    getCalls++;
    return 0;
  }
};

Object.defineProperty(Tuple.prototype, "length", desc);

var sample = Object(#[42, 43]);

var result = sample.slice();

assertEq(getCalls, 0, "ignores length properties");
assertEq(result, #[42, 43]);

reportCompare(0, 0);
