var count = 0;

var a = [0, 1];
for (var i in a) {
    assertEq(++count <= 1, true);
    a.shift();
}

