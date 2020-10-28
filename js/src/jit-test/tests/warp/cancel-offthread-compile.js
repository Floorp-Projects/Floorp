// |jit-test| --fast-warmup; skip-if: helperThreadCount() === 0

function foo(o) {
    return o.y;
}

with ({}) {}

var sum = 0;

// Trigger an off-thread Warp compile.
for (var i = 0; i < 30; i++) {
    sum += foo({y: 1});
}

// Attach a new stub and cancel that compile.
for (var i = 0; i < 30; i++) {
    sum += foo({x: 1, y: 1});
}

assertEq(sum, 60);
