var arr1 = [];
var arr2 = [];
for (let i = 0; i < 10; i++) {
    arr1.push(function f(x) {
        if (x === 350)
            bailout();
        assertEq(f, arr1[i]);
        return x + i;
    });
    arr2.push(function() {
        return arr1[i].apply(null, arguments);
    });
}

function test() {
    for (var i = 0; i < 400; i++) {
        for (var j = 0; j < arr2.length; j++) {
            assertEq(arr2[j].call(null, i), i + j);
        }
    }
}
test();
