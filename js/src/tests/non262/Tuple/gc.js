// |reftest| skip-if(!this.hasOwnProperty("Tuple"))

// Test that objects (in this case, closures) containing Tuples are traced properly
function foo() {
    var tup = #[1];
    (() => assertEq(tup, #[1]))();
}

let n = 1000;
for (i = 0; i < n; i++) {
    foo();
}

if (typeof reportCompare === "function") reportCompare(0, 0);
