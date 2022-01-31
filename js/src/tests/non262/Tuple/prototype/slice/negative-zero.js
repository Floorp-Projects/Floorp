// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

var sample = #[40,41,42,43];

assertEq(sample.slice(-0), sample);
assertEq(sample.slice(-0, 4), sample);
assertEq(sample.slice(0, -0), #[]);
assertEq(sample.slice(-0, -0), #[]);

reportCompare(0, 0);
