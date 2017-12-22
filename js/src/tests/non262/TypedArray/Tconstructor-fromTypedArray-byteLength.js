var g = newGlobal();

var arr = [1, 2, 3];
for (var constructor of anyTypedArrayConstructors) {
    var tarr = new constructor(arr);
    for (var constructor2 of anyTypedArrayConstructors) {
        var copied = new constructor2(tarr);
        assertEq(copied.buffer.byteLength, arr.length * constructor2.BYTES_PER_ELEMENT);

        g.tarr = tarr;
        copied = g.eval(`new ${constructor2.name}(tarr);`);
        assertEq(copied.buffer.byteLength, arr.length * constructor2.BYTES_PER_ELEMENT);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
