// Always use the per-element barrier.
gczeal(12);

function f() {
    var arr = [];
    for (var i = 0; i < 1000; i++)
        arr.push(i);
    gc(); // Ensure arr is tenured.

    for (var i = 0; i < 10; i++)
        arr.shift();

    // Add a nursery object, shift all elements, and trigger a GC to ensure
    // the post barrier doesn't misbehave.
    for (var j = 0; j < 40; j++)
        arr[500] = {x: j};
    while (arr.length > 0)
        arr.shift();

    gc();
    return arr;
}
f();
