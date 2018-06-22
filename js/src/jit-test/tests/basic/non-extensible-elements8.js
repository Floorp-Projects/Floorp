function testSealNonExtensibleArray() {
    var obj = Object.preventExtensions([0]);
    var desc = {value: 0, writable: true, enumerable: true, configurable: true};

    var errorAt = -1;
    var N = 100;

    for (var i = 0; i <= N; ++i) {
        if (i === N) {
            Object.seal(obj);
        }
        try {
            Object.defineProperty(obj, 0, desc);
        } catch {
            errorAt = i;
        }
    }
    assertEq(errorAt, N);
}
for (var i = 0; i < 15; i++)
    testSealNonExtensibleArray();

function testSealNonExtensibleObject() {
    var obj = Object.preventExtensions({0:1});
    var desc = {value: 0, writable: true, enumerable: true, configurable: true};

    var errorAt = -1;
    var N = 100;

    for (var i = 0; i <= N; ++i) {
        if (i === N) {
            Object.seal(obj);
        }
        try {
            Object.defineProperty(obj, 0, desc);
        } catch {
            errorAt = i;
        }
    }
    assertEq(errorAt, N);
}
for (var i = 0; i < 15; i++)
    testSealNonExtensibleObject();
