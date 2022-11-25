// |jit-test| --fast-warmup

var sum = 0;
function foo(o) {
    sum += o.a;
}

with({}) {}

// Fold a stub with two cases.
for (var i = 0; i < 11; i++) {
    foo({a:1});
    foo({a:1,b:2});
}

// Add a third case.
foo({a:1,b:2,c:3});

// Warp-compile.
for (var i = 0; i < 16; i++) {
    foo({a:1});
}

// Add a fourth case.
foo({a:1,b:2,c:3,d:4});

// Run for a while in Warp.
for (var i = 0; i < 20; i++) {
    foo({a:1});
    foo({a:1,b:2});
    foo({a:1,b:2,c:3});
    foo({a:1,b:2,c:3,d:4});
}

assertEq(sum, 120);
