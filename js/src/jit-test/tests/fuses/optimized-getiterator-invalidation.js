
const ITERS = 1000;

// A function which when warp compiled should use
// OptimizedGetIterator elision, and rely on invalidation
function f(x) {
    let sum = 0;
    for (let i = 0; i < ITERS; i++) {
        const [a, b, c] = x
        sum = a + b + c;
    }
    return sum
}

// Run the function f 1000 times to warp compile it. Use 4 elements here to ensure
// the return property of the ArrayIteratorPrototype is called.
let arr = [1, 2, 3, 4];
for (let i = 0; i < 1000; i++) {
    f(arr);
}

// Initialize the globally scoped counter
let counter = 0;
const ArrayIteratorPrototype = Object.getPrototypeOf([][Symbol.iterator]());

// Setting the return property should invalidate the warp script here.
ArrayIteratorPrototype.return = function () {
    counter++;
    return { done: true };
};


// Call f one more time
f(arr);

// Use assertEq to check the value of counter.
assertEq(counter, ITERS);
