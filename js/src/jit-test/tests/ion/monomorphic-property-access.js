function Foo() {
    for (var i=0; i<10; i++) {
        this["p" + i] = i;
    }
}

function test1(foo) {
    for (var i=0; i<10400; i++) {
        foo.p1 = i;
        foo.p9 = i;
        var x = foo.p0 + foo.p1 + foo.p2 + foo.p8 + foo.p4 +
            foo.p5 + foo.p6 + foo.p7 + foo.p3 + foo.p9;
        assertEq(x, i + i + 35);
    }
}

test1(new Foo);

function Bar(arg) {
    if (arg) { // Thwart definite-property analysis.
        this.x = 1;
        this.y = 2;
        this.z = 3;
    }
}

function test2(bar) {
    for (var i=0; i<10400; i++) {
        bar.x++;
        bar.y++;
        bar.z++;
    }
    assertEq(bar.x, 10401);
    assertEq(bar.y, 10402);
    assertEq(bar.z, 10403);
}

test2(new Bar(true));
