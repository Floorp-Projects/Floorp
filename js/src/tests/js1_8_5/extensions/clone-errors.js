// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function check(v) {
    try {
        serialize(v);
    } catch (exc) {
        return;
    }
    throw new Error("serializing " + uneval(v) + " should have failed with an exception");
}

// Unsupported object types.
check(new Error("oops"));
check(this);
check(Math);
check(function () {});
check(Proxy.create({enumerate: function () { return []; }}));
check(<element/>);
check(new Namespace("x"));
check(new QName("x", "y"));

// A failing getter.
check({get x() { throw new Error("fail"); }});

// Various recursive objects, i.e. those which the structured cloning
// algorithm wants us to reject due to "memory".
//
// Recursive array.
var a = [];
a[0] = a;
check(a);

// Recursive Object.
var b = {};
b.next = b;
check(b);

// Mutually recursive objects.
a[0] = b;
b.next = a;
check(a);
check(b);

// A recursive object that doesn't fail until 'memory' contains lots of objects.
a = [];
b = a;
for (var i = 0; i < 10000; i++) {
    b[0] = {};
    b[1] = [];
    b = b[1];
}
b[0] = {owner: a};
b[1] = [];
check(a);

reportCompare(0, 0, "ok");
