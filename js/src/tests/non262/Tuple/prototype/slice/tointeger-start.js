// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.3.5
...
4. Let relativeStart be ? ToInteger(start).
*/

var obj = {
  valueOf: function() {
    return 2;
  }
};


var sample = #[40, 41, 42, 43];

assertEq(sample.slice(false), sample);
assertEq(sample.slice(true), #[41, 42, 43]);

assertEq(sample.slice(NaN), sample);
assertEq(sample.slice(null), sample);
assertEq(sample.slice(undefined), sample);

assertEq(sample.slice(0.6), sample);
assertEq(sample.slice(1.1), #[41, 42, 43]);
assertEq(sample.slice(1.5), #[41, 42, 43]);
assertEq(sample.slice(-0.6), sample);
assertEq(sample.slice(-1.1), #[43]);
assertEq(sample.slice(-1.5), #[43]);

assertEq(sample.slice("3"), #[43]);

assertEq(sample.slice(obj), #[42, 43]);

reportCompare(0, 0);
