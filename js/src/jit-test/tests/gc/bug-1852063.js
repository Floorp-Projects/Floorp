// |jit-test| --fast-warmup

gczeal(0);

// Create a gray function.
grayRoot()[0] = (obj) => obj.x;

function foo(obj, skip) {
  if (!skip)
    return grayRoot()[0](obj);
}

with ({}) {}

// Set up `foo` to inline the gray function when we hit the threshold.
for (var i = 0; i < 6; i++) {
  foo({x:1}, false);
  foo({y:1, x:2}, false);
}

// Start a gc, yielding after marking gray roots.
gczeal(25);
startgc(1);

// Trigger inlining, being careful not to call and mark the gray function.
// This adds the gray function to cellsToAssertNotGray.
for (var i = 0; i < 10; i++) {
  foo({x:1}, true);
}

// Finish the gc and process the delayed gray checks list.
finishgc();
