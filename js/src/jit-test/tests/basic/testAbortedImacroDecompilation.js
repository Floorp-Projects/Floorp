function f() {
    for (var i=0; i<9; i++)
        assertEq("" + f, expected);
}

var expected = "" + f;
f();
