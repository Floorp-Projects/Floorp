// Tests when JSOP_OPTIMIZE_SPREADCALL no longer apply after the initial Ion
// compilation.

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

// The rest arguments don't share a common prototype.
function test() {
    class MyArray1 extends Array { }
    class MyArray2 extends Array { }
    function maybeInvalidate(rest) {
        // Use a WithStatement to prevent Ion-inlining. This ensures any
        // bailouts due to type changes don't occur in this function, but
        // instead in the caller.
        with ({});

        if (i >= 1900) {
            if (i & 1)
                rest = new MyArray1(3, 4);
            else
                rest = new MyArray2(5, 6);
        }
        return rest;
    }
    function fn(...rest) {
        rest = maybeInvalidate(rest);
        return add(...rest);
    }
    for (var i = 0; i < 4000; ++i) {
        assertEq(fn(1, 2), i < 1900 ? 3 : (i & 1) ? 7 : 11);
    }
}
test();
