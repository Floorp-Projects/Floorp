function test() {
    var from, to;

    // From values.
    from = {x: 1, y: 2};
    ({...to} = from);
    assertEq(to.y, 2);

    var z;
    from = {x: 1, y: 2};
    ({x: z, ...to} = from);
    assertEq(z, 1);
    assertEq(to.y, 2);

    // From getter.
    var c = 7;
    from = {x: 1, get y() { return ++c; }};
    ({...to} = from);
    assertEq(c, 8);
    assertEq(to.y, 8);

    from = {x: 1, get y() { return ++c; }};
    ({y: z, ...to} = from);
    assertEq(c, 9);
    assertEq(z, 9);
    assertEq(to.y, undefined);

    // Array with dense elements.
    from = [1, 2, 3];
    ({...to} = from);
    assertEq(to[2], 3);
    assertEq("length" in to, false);

    from = [1, 2, 3];
    ({2: z, ...to} = from);
    assertEq(z, 3);
    assertEq(to[2], undefined);
    assertEq(to[0], 1);
    assertEq("length" in to, false);

    // Object with sparse elements and symbols.
    from = {x: 1, 1234567: 2, 1234560: 3, [Symbol.iterator]: 5, z: 3};
    ({...to} = from);
    assertEq(to[1234567], 2);
    assertEq(Object.keys(to).toString(), "1234560,1234567,x,z");
    assertEq(to[Symbol.iterator], 5);

    from = {x: 1, 1234567: 2, 1234560: 3, [Symbol.iterator]: 5, z: 3};
    ({[Symbol.iterator]: z, ...to} = from);
    assertEq(to[1234567], 2);
    assertEq(Object.keys(to).toString(), "1234560,1234567,x,z");
    assertEq(to[Symbol.iterator], undefined);
    assertEq(z, 5);

    // Typed array.
    from = new Int32Array([1, 2, 3]);
    ({...to} = from);
    assertEq(to[1], 2);

    from = new Int32Array([1, 2, 3]);
    ({1: z, ...to} = from);
    assertEq(z, 2);
    assertEq(to[1], undefined);
    assertEq(to[2], 3);

    // Primitive string.
    from = "foo";
    ({...to} = from);
    assertEq(to[0], "f");

    from = "foo";
    ({0: z, ...to} = from);
    assertEq(z, "f");
    assertEq(to[0], undefined);
    assertEq(to[1], "o");

    // String object.
    from = new String("bar");
    ({...to} = from);
    assertEq(to[2], "r");

    from = new String("bar");
    ({1: z, ...to} = from);
    assertEq(z, "a");
    assertEq(to[1], undefined);
    assertEq(to[2], "r");
}
test();
test();
test();
