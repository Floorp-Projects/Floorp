// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

// If an indexed Array setter is overridden, TupleSplice shouldn't use it
// when constructing the intermediate array

var z = 5;
print("1111");
Object.defineProperty(Array.prototype, '0', { set: function(y) { z = 42; }});
print("2222");
let newT = #[1,2,3].toSpliced(0, 2);
print("3333");
assertEq(z, 5);
print("4444");

reportCompare(0, 0);

