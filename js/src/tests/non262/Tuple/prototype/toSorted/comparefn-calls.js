// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var expectedThis = (function() {
  return this;
})();


var sample = #[42, 42, 42, 42, 42];
var calls = [];

var comparefn = function() {
    calls.push([this, arguments]);
};

let result = sample.toSorted(comparefn);

assertEq(calls.length > 0, true);

calls.forEach(function(args) {
    assertEq(args[0], expectedThis);
    assertEq(args[1].length, 2);
    assertEq(args[1][0], 42);
    assertEq(args[1][0], 42);
});

reportCompare(0, 0);
