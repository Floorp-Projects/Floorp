// Iterator prototype surfaces.

load(libdir + "asserts.js");

function test(constructor) {
    var proto = Object.getPrototypeOf(constructor().iterator());
    var names = Object.getOwnPropertyNames(proto);
    assertEq(names.length, 1);
    assertEq(names[0], 'next');

    var desc = Object.getOwnPropertyDescriptor(proto, 'next');
    assertEq(desc.configurable, true);
    assertEq(desc.enumerable, false);
    assertEq(desc.writable, true);

    assertEq(proto.iterator(), proto);
    assertThrowsValue(function () { proto.next(); }, StopIteration);
}

//test(Array);
test(Map);
test(Set);
