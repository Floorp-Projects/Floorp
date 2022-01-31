// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
/*
8.2.3.5
...
6. If end is undefined, let relativeEnd be len; else let relativeEnd be ? ToInteger(end).
*/

var obj = {
  valueOf: function() {
    return 2;
  }
};


var sample = #[40, 41, 42, 43];

assertEq(sample.slice(0, false), #[]);
assertEq(sample.slice(0, true), #[40]);

assertEq(sample.slice(0, NaN), #[]);
assertEq(sample.slice(0, null), #[]);
assertEq(sample.slice(0, undefined), #[40, 41, 42, 43]);

assertEq(sample.slice(0, 0.6), #[]);
assertEq(sample.slice(0, 1.1), #[40]);
assertEq(sample.slice(0, 1.5), #[40]);
assertEq(sample.slice(0, -0.6), #[]);
assertEq(sample.slice(0, -1.1), #[40, 41, 42]);
assertEq(sample.slice(0, -1.5), #[40, 41, 42]);

assertEq(sample.slice(0, "3"), #[40, 41, 42]);

assertEq(sample.slice(0, obj), #[40, 41]);

reportCompare(0, 0);
