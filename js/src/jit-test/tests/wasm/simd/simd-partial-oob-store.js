// |jit-test| skip-if: !wasmSimdEnabled()

// Cloned from ad-hack.js but kept separate because it may have to be disabled
// on some devices until bugs are fixed.

// Bug 1666747 - partially OOB stores are not handled correctly on ARM and ARM64.
// The simulators don't implement the correct semantics anyhow, so when the bug
// is fixed in the code generator they must remain excluded here.
var conf = getBuildConfiguration();
if (conf.arm64 || conf["arm64-simulator"] || conf.arm || conf["arm-simulator"])
    quit(0);

function get(arr, loc, len) {
    let res = [];
    for ( let i=0; i < len; i++ ) {
        res.push(arr[loc+i]);
    }
    return res;
}

function iota(len) {
    let xs = [];
    for ( let i=0 ; i < len ; i++ )
        xs.push(i);
    return xs;
}

function assertSame(got, expected) {
    assertEq(got.length, expected.length);
    for ( let i=0; i < got.length; i++ ) {
        let g = got[i];
        let e = expected[i];
        assertEq(g, e);
    }
}

for ( let offset of iota(16) ) {
    var ins = wasmEvalText(`
  (module
    (memory (export "mem") 1 1)
    (func (export "f") (param $loc i32)
      (v128.store offset=${offset} (local.get $loc) (v128.const i32x4 ${1+offset} 2 3 ${4+offset*2}))))`);

    // OOB write should trap
    assertErrorMessage(() => ins.exports.f(65536-15),
                       WebAssembly.RuntimeError,
                       /index out of bounds/)

    // Ensure that OOB writes don't write anything.
    let start = 65536 - 15 + offset;
    let legalBytes = 65536 - start;
    var mem8 = new Uint8Array(ins.exports.mem.buffer);
    assertSame(get(mem8, start, legalBytes), iota(legalBytes).map((_) => 0));
}
