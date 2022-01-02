var sum = 0;

function foo() { sum++; }

const MAX_JIT_ARGS = 4096 / 8;
var arr = [];
for (var i = 0; i < MAX_JIT_ARGS + 1; i++) {
    arr.push(1);
}

for (var i = 0; i < 275; i++) {
    foo(...arr);
}
assertEq(sum, 275);
