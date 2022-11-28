var sum = 0;
function foo(o) {
    sum += o.x;
}

with({}) {}

// Trigger stub folding in MaybeTransition
for (var i = 0; i < 200; i++) {
    foo({x:1, a:1});
    foo({x:1, b:1});
    foo({x:1, c:1});
    foo({x:1, d:1});
    foo({x:1, e:1});
    foo({x:1, f:1});
    foo({x:1, g:1});
    foo({x:1, h:1});
    foo({x:1, i:1});
    foo({x:1, j:1});
    foo({x:1, k:1});
    foo({x:1, l:1});
}

assertEq(sum, 2400);
