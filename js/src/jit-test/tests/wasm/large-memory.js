// |jit-test| skip-if: !largeArrayBufferEnabled(); --large-arraybuffers

var pagesz = PageSizeInBytes;
var pages_limit = MaxPagesIn32BitMemory;

// 40000 is well above 2GB but not anything that might run into boundary
// conditions near 4GB.
var pages_vanilla = 40000;

for ( let [pages,maxpages] of [[pages_vanilla, pages_vanilla+100],
                               [pages_limit, pages_limit]] ) {
    assertEq(pages == maxpages || maxpages - pages >= 3, true)

    let ins = wasmEvalText(`
(module
  (memory (export "mem") ${pages} ${maxpages})

  (data (i32.const ${(pages-5)*pagesz}) "yabbadabbado")
  (data $flintstone passive "yabbadabbado")

  (func (export "get_constaddr") (result i32)
    (i32.load (i32.const ${pages*pagesz-4})))

  (func (export "get_varaddr") (param $p i32) (result i32)
    (i32.load (local.get $p)))

  (func (export "set_constaddr") (param $v i32)
    (i32.store (i32.const ${pages*pagesz-8}) (local.get $v)))

  (func (export "set_varaddr") (param $p i32) (param $v i32)
    (i32.store (local.get $p) (local.get $v)))

  (func (export "get_constaddr_large_offset") (result i32)
    (i32.load offset=${(pages-100)*pagesz-4} (i32.const ${pagesz*100})))

  (func (export "get_varaddr_large_offset") (param $p i32) (result i32)
    (i32.load offset=${(pages-100)*pagesz-4} (local.get $p)))

  (func (export "get_constaddr_small_offset") (result i32)
    (i32.load offset=${pagesz-4} (i32.const ${(pages-1)*pagesz})))

  (func (export "get_varaddr_small_offset") (param $p i32) (result i32)
    (i32.load offset=${pagesz-4} (local.get $p)))

  (func (export "set_constaddr_large_offset") (param $v i32)
    (i32.store offset=${(pages-100)*pagesz-16} (i32.const ${pagesz*100}) (local.get $v)))

  (func (export "set_varaddr_large_offset") (param $p i32) (param $v i32)
    (i32.store offset=${(pages-100)*pagesz-20} (local.get $p) (local.get $v)))

  (func (export "set_constaddr_small_offset") (param $v i32)
    (i32.store offset=${pagesz-24} (i32.const ${(pages-1)*pagesz}) (local.get $v)))

  (func (export "set_varaddr_small_offset") (param $p i32) (param $v i32)
    (i32.store offset=${pagesz-28} (local.get $p) (local.get $v)))

  (func (export "copy") (param $dest i32) (param $src i32)
    (memory.copy (local.get $dest) (local.get $src) (i32.const 12)))

  (func (export "init") (param $dest i32)
    (memory.init $flintstone (local.get $dest) (i32.const 0) (i32.const 12)))

  (func (export "fill") (param $dest i32) (param $val i32) (param $len i32)
    (memory.fill (local.get 0) (local.get 1) (local.get 2)))

  (func (export "grow1") (result i32)
    (memory.grow (i32.const 1)))
)`);

    let buf = new Int32Array(ins.exports.mem.buffer);

    let checkFlintstoneAt = function (addr) {
        assertEq(buf[addr/4], 0x62626179)   // "yabb", little-endian
        assertEq(buf[addr/4+1], 0x62616461) // "adab", ditto
        assertEq(buf[addr/4+2], 0x6f646162) // "bado"
    }

    buf[pages*pagesz/4-1] = 0xdeadbeef;
    assertEq(ins.exports.get_constaddr(), 0xdeadbeef|0);
    assertEq(ins.exports.get_varaddr(pages*pagesz-4), 0xdeadbeef|0);

    assertEq(ins.exports.get_constaddr_large_offset(), 0xdeadbeef|0);
    assertEq(ins.exports.get_varaddr_large_offset(pagesz*100), 0xdeadbeef|0);

    assertEq(ins.exports.get_constaddr_small_offset(), 0xdeadbeef|0);
    assertEq(ins.exports.get_varaddr_small_offset((pages-1)*pagesz), 0xdeadbeef|0);

    ins.exports.set_constaddr(0xcafebab0);
    assertEq(buf[pages*pagesz/4-2], 0xcafebab0|0);

    ins.exports.set_varaddr(pages*pagesz-12, 0xcafebab1);
    assertEq(buf[pages*pagesz/4-3], 0xcafebab1|0);

    ins.exports.set_constaddr_large_offset(0xcafebab2);
    assertEq(buf[pages*pagesz/4-4], 0xcafebab2|0);

    ins.exports.set_varaddr_large_offset(pagesz*100, 0xcafebab3);
    assertEq(buf[pages*pagesz/4-5], 0xcafebab3|0);

    ins.exports.set_constaddr_small_offset(0xcafebab4);
    assertEq(buf[pages*pagesz/4-6], 0xcafebab4|0);

    ins.exports.set_varaddr_small_offset((pages-1)*pagesz, 0xcafebab5);
    assertEq(buf[pages*pagesz/4-7], 0xcafebab5|0);

    assertErrorMessage(() => ins.exports.get_varaddr(pages*pagesz),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);

    assertErrorMessage(() => ins.exports.get_varaddr_large_offset(pagesz*100+4),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);

    assertErrorMessage(() => ins.exports.get_varaddr_small_offset((pages-1)*pagesz+4),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);

    ins.exports.set_varaddr(pages*pagesz-4, 0); // Should work
    assertErrorMessage(() => ins.exports.set_varaddr(pages*pagesz, 0),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);

    ins.exports.set_varaddr_large_offset(pagesz*100+16, 0); // Should work
    assertErrorMessage(() => ins.exports.set_varaddr_large_offset(pagesz*100+20, 0),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);

    ins.exports.set_varaddr_small_offset((pages-1)*pagesz+24); // Should work
    assertErrorMessage(() => ins.exports.set_varaddr_small_offset((pages-1)*pagesz+28, 0),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);

    // Active init
    checkFlintstoneAt((pages-5)*pagesz);

    // memory.init
    ins.exports.init((pages-6)*pagesz);
    checkFlintstoneAt((pages-6)*pagesz);

    ins.exports.init(pages*pagesz-12); // Should work
    // Dest goes OOB
    assertErrorMessage(() => ins.exports.init(pages*pagesz-6),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);

    // memory.copy

    // Dest and src are in bounds
    ins.exports.copy((pages-10)*pagesz, (pages-5)*pagesz);
    checkFlintstoneAt((pages-10)*pagesz);

    // Dest goes OOB
    assertErrorMessage(() => ins.exports.copy((pages)*pagesz-6, (pages-5)*pagesz, 12),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);

    ins.exports.copy((pages)*pagesz-12, (pages-1)*pagesz); // Should work
    // Src goes OOB
    assertErrorMessage(() => ins.exports.copy((pages)*pagesz-12, (pages)*pagesz-6),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);


    // memory.fill
    let lastpg = (pages-1)*pagesz;
    ins.exports.fill(lastpg, 0x37, pagesz);
    for ( let i=0; i < pagesz/4; i++ )
        assertEq(buf[lastpg/4+i], 0x37373737);

    assertErrorMessage(() => ins.exports.fill(lastpg, 0x42, pagesz+1),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);

    if (pages < maxpages) {
        assertEq(ins.exports.grow1(), pages);
        assertEq(ins.exports.grow1(), pages+1);
        assertEq(ins.exports.grow1(), pages+2);

        assertEq(ins.exports.get_varaddr((pages+2)*pagesz), 0);

        let i = 0;
        while (ins.exports.grow1() != -1) {
            i++;
        }
        assertEq(i, maxpages-pages-3);
    }
}

// Very large offsets should be allowed (but may of course be OOB).  Observe
// that offset=0xffffffff is accepted by the validator even though this access
// could never succeed.
{
    let ins = wasmEvalText(`
(module
  (memory (export "mem") 1)
  (func (export "get1") (param $p i32) (result i32)
    (i32.load offset=0x80000000 (local.get $p)))
  (func (export "get2") (param $p i32) (result i32)
    (i32.load offset=0xfffffffc (local.get $p)))
  (func (export "get3") (param $p i32) (result i32)
    (i32.load offset=0xffffffff (local.get $p))))
`);
    assertErrorMessage(() => ins.exports.get1(0),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);
    assertErrorMessage(() => ins.exports.get2(0),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);
    assertErrorMessage(() => ins.exports.get3(0),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);
}

{
    let ins = wasmEvalText(`
(module
  (memory (export "mem") 32768)
  (func (export "get1") (param $p i32) (result i32)
    (i32.load8_s offset=0x7fffffff (local.get $p))))
`);
    assertEq(ins.exports.get1(0), 0);
    assertErrorMessage(() => ins.exports.get1(1),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);
    assertErrorMessage(() => ins.exports.get1(-1),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);
}

{
    let ins = wasmEvalText(`
(module
  (memory (export "mem") 32769)
  (func (export "get1") (param $p i32) (result i32)
    (i32.load8_s offset=0x80000000 (local.get $p))))
`);
    assertEq(ins.exports.get1(0), 0);
    assertEq(ins.exports.get1(65535), 0);
    assertErrorMessage(() => ins.exports.get1(65536),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);
}

{
    let ins = wasmEvalText(`
(module
  (memory (export "mem") ${pages_limit})
  (func (export "get1") (param $p i32) (result i32)
    (i32.load8_s offset=${pages_limit*pagesz-1} (local.get $p))))
`);
    assertEq(ins.exports.get1(0), 0);
    assertErrorMessage(() => ins.exports.get1(1),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);
    assertErrorMessage(() => ins.exports.get1(-1),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);
}

// Fail to grow at the declared max
// Import and export

{
    let ins = wasmEvalText(`
(module
  (memory (export "mem") ${pages_limit} ${pages_limit})

  (func (export "set_it") (param $addr i32) (param $val i32)
    (i32.store (local.get $addr) (local.get $val)))

  (func (export "grow1") (result i32)
    (memory.grow (i32.const 1)))
)`);

    assertEq(ins.exports.grow1(), -1); // Because initial == max

    let ins2 = wasmEvalText(`
(module
  (import "" "mem" (memory 1))

  (func (export "get_it") (param $addr i32) (result i32)
    (i32.load (local.get $addr)))
)`,
                            {"":ins.exports})

    ins.exports.set_it((pages_limit*pagesz)-4, 0xbaadf00d);
    assertEq(ins2.exports.get_it((pages_limit*pagesz)-4), 0xbaadf00d|0)
}

// Fail to grow at the max heap size even if there is headroom
// in the declared max
if (pages_limit < 65536) {
    let ins = wasmEvalText(`
(module
  (memory (export "mem") ${pages_limit} ${pages_limit+2})

  (func (export "grow1") (result i32)
    (memory.grow (i32.const 1)))
)`);

    assertEq(ins.exports.grow1(), -1); // Because current is at the max heap size
}

// Fail to instantiate when the minimum is larger than the max heap size
{
    assertErrorMessage(() => wasmEvalText(`
(module (memory (export "mem") ${pages_limit+1} ${pages_limit+1}))
`),
                       WebAssembly.RuntimeError,
                       /too many memory pages/);
}
