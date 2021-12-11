function test1() {
    for (var i = 0; i <= 90; i++) {
        var x = i/10.0;
        if (Math.round(x) !== Math.floor(x + 0.5))
            assertEq(0, 1);
    }
}
test1();

function test2() {
    for (var i = -5; i >= -90; i--) {
        if (i === -5)
	    x = -0.500000000000001;
        else
	    x = i/10.0;

        if (Math.round(x) !== Math.floor(x + 0.5))
            assertEq(0, 1);
    }
}
test2();
