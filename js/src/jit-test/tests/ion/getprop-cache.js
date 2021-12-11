function test1() {
    function h(node) {
        var x = 0.1;
        for (var i=0; i<100; i++) {
            x += node.parent;
        }
        return x;
    }
    function build(depth) {
        if (depth > 10)
            return {parent: 3.3};
        return {__proto__: build(depth + 1)};
    }
    var tree = build(0);
    assertEq(h(tree)|0, 330);
}
test1();

function test2() {
    function Foo() {};
    Foo.prototype.x = 3.3;

    var o = new Foo();
    for (var i=0; i<100; i++) {
        assertEq(o.x, 3.3);
    }
}
test2();
