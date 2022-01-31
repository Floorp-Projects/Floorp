// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

// If an indexed Array setter is overridden, TupleConcat shouldn't use it
// when constructing the intermediate array

var z = 5;
for (i = 0; i < 4; i++) {
    Object.defineProperty(Array.prototype, i, { set: function(y) { z = 42; }});
}
var newT = #[1, 2].concat([3, 4]);
assertEq(z, 5);

newT = #[1, 2].concat("hello");
assertEq(z, 5);

reportCompare(0, 0);

