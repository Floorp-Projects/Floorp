load(libdir + "asm.js");
load(libdir + "simd.js");
load(libdir + "asserts.js");

// Set to true to see more JS debugging spew.
const DEBUG = false;

if (!isSimdAvailable()) {
    DEBUG && print("won't run tests as simd extensions aren't activated yet");
    quit(0);
}

// Test all bit-casts and normal loads and stores.
var heap = new ArrayBuffer(BUF_MIN);
var asU8 = new Uint8Array(heap);
var allTypes = [
    "Int8x16",
    "Int16x8",
    "Int32x4",
    "Uint8x16",
    "Uint16x8",
    "Uint32x4",
    "Float32x4"
];

// Generate a load bit-cast store test function that performs:
//
// function f(a, b) {
//     vec = src.load(H, a);
//     cast = dst.from«src»Bits(vec);
//     store(H, b, cast);
// }
//
// Here, `H` is the heap provided by `heap`.
function test_func(src, dst) {
    text = `
        "use asm";
        var src = glob.SIMD.${src};
        var dst = glob.SIMD.${dst};
        var ld = src.load;
        var st = dst.store;
        var bc = dst.from${src}Bits;

        var H = new glob.Uint8Array(heap);

        function f(a, b) {
            a = a|0;
            b = b|0;

            st(H, b, bc(ld(H, a)));
        }

        return f;
    `;
    return asmLink(asmCompile('glob', 'ffi', 'heap', text), this, null, heap);
}

function assertBuf16(a, b) {
    for (let i=0; i < 16; i++) {
        assertEq(asU8[a+i], asU8[b+i]);
    }
}

for (let src of allTypes) {
    for (let dst of allTypes) {
        // Skip identity conversions.
        if (src == dst) continue;

        print(src, dst);
        let f = test_func(src, dst);
        // Initialize with pseudo-random data.
        for (let i = 0; i < 64; i++) {
            asU8[i] = (i + 17) * 97;
        }

        // Aligned load/store.
        f(0, 16);
        assertBuf16(0, 16);

        // Unaligned access.
        f(1, 27);
        assertBuf16(1, 27);
    }
}
