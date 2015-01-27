
// Test various ways of converting an unboxed object to native.

function Foo(a, b) {
    this.a = a;
    this.b = b;
}

var proxyObj = {
  get: function(recipient, name) {
    return recipient[name] + 2;
  }
};

function f() {
    var a = [];
    for (var i = 0; i < 50; i++)
        a.push(new Foo(i, i + 1));

    var prop = "a";

    i = 0;
    for (; i < 5; i++)
        a[i].c = i;
    for (; i < 10; i++)
        Object.defineProperty(a[i], 'c', {value: i});
    for (; i < 15; i++)
        a[i] = new Proxy(a[i], proxyObj);
    for (; i < 20; i++)
        a[i].a = 3.5;
    for (; i < 25; i++)
        delete a[i].b;
    for (; i < 30; i++)
        a[prop] = 4;

    var total = 0;
    for (i = 0; i < a.length; i++) {
        if ('a' in a[i])
            total += a[i].a;
        if ('b' in a[i])
            total += a[i].b;
        if ('c' in a[i])
            total += a[i].c;
    }
    assertEq(total, 2382.5);
}
f();
