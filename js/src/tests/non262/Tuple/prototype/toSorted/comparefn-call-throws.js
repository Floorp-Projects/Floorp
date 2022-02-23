// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
let sample = #[42, 43, 44, 45, 46];
var calls = 0;

let comparefn = function() {
    calls += 1;
    throw new TypeError();
};

assertThrowsInstanceOf(function() {
    sample.toSorted(comparefn);
}, TypeError, "comparefn should throw");

assertEq(calls, 1);

reportCompare(0, 0);
