// Iterator prototype surfaces.

load(libdir + "asserts.js");
load(libdir + "iteration.js");

var iterProto = null;

function test(constructor) {
    var iter = new constructor()[Symbol.iterator]();
    assertDeepEq(Reflect.ownKeys(iter), []);

    // Iterator prototypes only have a .next property.
    // At least until we support @@toStringTag.
    var proto1 = Object.getPrototypeOf(iter);

    var names = Reflect.ownKeys(proto1);
    names.sort();
    assertDeepEq(Reflect.ownKeys(proto1), ['next']);

    var desc = Object.getOwnPropertyDescriptor(proto1, 'next');
    assertEq(desc.configurable, true);
    assertEq(desc.enumerable, false);
    assertEq(desc.writable, true);

    // %IteratorPrototype%
    var proto2 = Object.getPrototypeOf(proto1);
    assertEq(Object.getPrototypeOf(proto2), Object.prototype);
    assertEq(Object.prototype.toString.call(proto2), "[object Object]");

    assertDeepEq(Reflect.ownKeys(proto2), [Symbol.iterator]);
    assertEq(proto2[Symbol.iterator](), proto2);

    // Check there's a single %IteratorPrototype% object.
    if (iterProto === null)
	iterProto = proto2;
    else
	assertEq(iterProto, proto2);
}

test(Array);
test(String);
test(Map);
test(Set);
