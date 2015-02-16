
// Use the correct receiver when non-native objects are prototypes of other objects.

function Thing(a, b) {
    this.a = a;
    this.b = b;
}

function foo() {
    var array = [];
    for (var i = 0; i < 10000; i++)
        array.push(new Thing(i, i + 1));

    var proto = new Thing(1, 2);
    var obj = Object.create(proto);

    Object.defineProperty(Thing.prototype, "c", {set:function() { this.d = 0; }});
    obj.c = 3;
    assertEq(obj.c, undefined);
    assertEq(obj.d, 0);
    assertEq(obj.hasOwnProperty("d"), true);
    assertEq(proto.d, undefined);
    assertEq(proto.hasOwnProperty("d"), false);

    obj.a = 3;
    assertEq(obj.a, 3);
    assertEq(proto.a, 1);
    assertEq(obj.hasOwnProperty("a"), true);
}

foo();
