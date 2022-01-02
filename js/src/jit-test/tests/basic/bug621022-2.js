function f(j) {
    var a = [[1],[1],[1],[1],[1],[1],[1],[1],[1],arguments];
    var b;
    for (var i = 0; i < a.length; i++) {
        delete a[i][0];
        b = arguments[0];
    }
    assertEq(b === undefined, true);
}

f(1);


