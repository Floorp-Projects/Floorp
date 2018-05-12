function testTestIntegrityLevel(a, hasElems) {
    assertEq(Object.isExtensible(a), true);
    assertEq(Object.isSealed(a), false);
    assertEq(Object.isFrozen(a), false);

    Object.preventExtensions(a);
    assertEq(Object.isExtensible(a), false);
    assertEq(Object.isSealed(a), !hasElems);
    assertEq(Object.isFrozen(a), false);

    Object.seal(a);
    assertEq(Object.isExtensible(a), false);
    assertEq(Object.isSealed(a), true);
    assertEq(Object.isFrozen(a), false);

    Object.freeze(a);
    assertEq(Object.isExtensible(a), false);
    assertEq(Object.isSealed(a), true);
    assertEq(Object.isFrozen(a), true);
}
testTestIntegrityLevel([1, 2, 3], true);
testTestIntegrityLevel([1, , , 2], true);
testTestIntegrityLevel([1, , , ], true);
testTestIntegrityLevel([, , , ], false);
testTestIntegrityLevel([], false);
testTestIntegrityLevel({0: 0}, true);
var a = [,,,,,,, 1];
a.pop();
testTestIntegrityLevel(a, false);

function testDescriptor() {
    var a = [1];
    Object.preventExtensions(a);
    assertEq(JSON.stringify(Object.getOwnPropertyDescriptors(a)),
             `{"0":{"value":1,"writable":true,"enumerable":true,"configurable":true},` +
             `"length":{"value":1,"writable":true,"enumerable":false,"configurable":false}}`);

    a = [1];
    Object.seal(a);
    assertEq(JSON.stringify(Object.getOwnPropertyDescriptors(a)),
             `{"0":{"value":1,"writable":true,"enumerable":true,"configurable":false},` +
             `"length":{"value":1,"writable":true,"enumerable":false,"configurable":false}}`);

    a = [1];
    Object.freeze(a);
    assertEq(JSON.stringify(Object.getOwnPropertyDescriptors(a)),
             `{"0":{"value":1,"writable":false,"enumerable":true,"configurable":false},` +
             `"length":{"value":1,"writable":false,"enumerable":false,"configurable":false}}`);
}
testDescriptor();
