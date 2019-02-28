// Tests when JSOP_OPTIMIZE_SPREADCALL can't be applied during the initial
// Ion compilation.

// JSOP_OPTIMIZE_SPREADCALL can be optimised when the following conditions
// are fulfilled:
//   (1) the argument is an array
//   (2) the array has no hole
//   (3) array[@@iterator] is not modified
//   (4) the array's prototype is Array.prototype
//   (5) Array.prototype[@@iterator] is not modified
//   (6) %ArrayIteratorPrototype%.next is not modified

function add(a, b) {
    return a + b;
}

// The rest argument's prototype isn't Array.prototype.
function test() {
    class MyArray extends Array { }
    function fn(...rest) {
        rest = new MyArray(3, 4);
        return add(...rest);
    }
    for (var i = 0; i < 2000; ++i) {
        assertEq(fn(1, 2), 7);
    }
}
test();
