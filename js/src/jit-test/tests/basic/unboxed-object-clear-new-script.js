
function Foo(a, b) {
    this.a = a;
    this.b = b;
}

function invalidate_foo() {
    var a = [];
    var counter = 0;
    for (var i = 0; i < 50; i++)
        a.push(new Foo(i, i + 1));
    Object.defineProperty(Foo.prototype, "a", {configurable: true, set: function() { counter++; }});
    for (var i = 0; i < 50; i++)
        a.push(new Foo(i, i + 1));
    delete Foo.prototype.a;
    var total = 0;
    for (var i = 0; i < a.length; i++) {
        assertEq('a' in a[i], i < 50);
        total += a[i].b;
    }
    assertEq(total, 2550);
    assertEq(counter, 50);
}
invalidate_foo();

function Bar(a, b, fn) {
    this.a = a;
    if (b == 30)
        Object.defineProperty(Bar.prototype, "b", {configurable: true, set: fn});
    this.b = b;
}

function invalidate_bar() {
    var a = [];
    var counter = 0;
    function fn() { counter++; }
    for (var i = 0; i < 50; i++)
        a.push(new Bar(i, i + 1, fn));
    delete Bar.prototype.b;
    var total = 0;
    for (var i = 0; i < a.length; i++) {
        assertEq('a' in a[i], true);
        assertEq('b' in a[i], i < 29);
        total += a[i].a;
    }
    assertEq(total, 1225);
    assertEq(counter, 21);
}
invalidate_bar();
