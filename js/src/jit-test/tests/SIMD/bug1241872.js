if (typeof SIMD !== 'object')
    exit(0);

function test() {
    return SIMD.Float32x4().toSource();
}

var r = '';
for (var i = 0; i < 10000; i++)
    r = test();
