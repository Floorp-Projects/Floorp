
function testInt8Array(L) {
    var f = new Int8Array(8);
    f[0] = 0;          // Don't assert.
}

for (var i = 0; i < 10; i++) {
    testInt8Array(0);
}


