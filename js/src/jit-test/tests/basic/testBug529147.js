var magicNumbers = [1, -1, 0, 0];
var magicIndex = 0;

var sum = 0;

function foo(n) {
    for (var i = 0; i < n; ++i) {
        sum += 10;
        bar();
    }
}

function bar() {
    var q = magicNumbers[magicIndex++];
    if (q != -1) {
        sum += 1;
        foo(q);
    }
}

foo(3);
assertEq(sum, 43);
