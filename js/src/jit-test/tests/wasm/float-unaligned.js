// Various tests for unaligned float accesses.  These are specifically meant to
// test the SIGBUS handling on 32-bit ARM by exercising odd addresses and odd
// offsets.

// For a triple of (numBallast, ty, offset), create the text for a pair of
// functions "get_ty_offset" and "set_ty_offset" where each has numBallast live
// dummy values across the operation of interest to force the use of different
// register numbers.  (This is primarily for the FP registers as ARM code
// generation currently always uses the same scratch register for the base
// address of the access.)
//
// These must be augmented with a memory.  Memory addresses 0-255 are reserved
// for internal use by these functions.  The memory must start as zero.

function makeLoadStore(numBallast, ty, offset) {
    // The general idea of the ballast is that we occupy some FP registers and
    // some int registers with non-dead values before we perform an operation,
    // and then we consume the occupied registers after.
    //
    // In the case of load, the loaded result is stored back in memory before we
    // consume the ballast, thus the ion regalloc will not simply always load
    // the result into d0, but usually into some temp other than d0.  Thus the
    // amount of ballast affects the register.  (Ditto baseline though the
    // reasoning is simpler.)
    //
    // In the case of store, we keep the parameter value live until the end so
    // that the tmp that we compute for the store is moved into a different
    // register.  The tmp has the same value as the parameter value but a
    // non-JIT compiler can't know that.

    let loadtxt =
      `(func (export "get_${ty}_${offset}") (param $p i32) (result ${ty})
         ${ballast(() => `
              (i32.const 8)
              (i32.store (i32.const 8) (i32.add (i32.load (i32.const 8)) (i32.const 1)))
              (${ty}.load (i32.const 8))`)}

         (${ty}.store (i32.const 0) (${ty}.load offset=${offset} (local.get $p)))

         ${ballast(() => `
             ${ty}.store`)}

         (${ty}.load (i32.const 0)))`;

    // This will assume the value at mem[16] is zero.
    let storetxt =
      `(func (export "set_${ty}_${offset}") (param $p i32) (param $v ${ty})
         (local $tmp ${ty})
         ${ballast(() => `
              (i32.const 8)
              (i32.store (i32.const 8) (i32.add (i32.load (i32.const 8)) (i32.const 1)))
              (${ty}.load (i32.const 8))`)}

         (local.set $tmp (${ty}.add (local.get $v) (${ty}.load (i32.const 16))))
         (${ty}.store offset=${offset} (local.get $p) (local.get $tmp))

         ${ballast(() => `
             ${ty}.store`)}
         (${ty}.store (i32.const 8) (local.get $v)))`;

    return `${loadtxt}
            ${storetxt}`;

    function ballast(thunk) {
        let s = "";
        for ( let i=0 ; i < numBallast; i++ )
            s += thunk();
        return s;
    }
}

// The complexity here comes from trying to force the source/target FP registers
// in the FP access instruction to vary.  For Baseline this is not hard; for Ion
// trickier.

function makeInstance(numBallast, offset) {
    let txt =
        `(module
           (memory (export "memory") 1 1)
           ${makeLoadStore(numBallast, 'f64', offset)}
           ${makeLoadStore(numBallast, 'f32', offset)})`;
    return new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(txt)));
}

// `offset` corresponds to the "offset" directive in the instruction
for ( let offset=0 ; offset < 8; offset++ ) {

    // `numBallast` represents the amount of ballast registers we're trying to use,
    // see comments above.
    for ( let numBallast=0; numBallast < 16; numBallast++ ) {
        let ins = makeInstance(numBallast, offset);
        let mem = ins.exports.memory;
        let buf = new DataView(mem.buffer);

        // `i` represents the offset in the pointer from a proper boundary
        for ( let i=0; i < 9; i++ ) {
	    let offs = 256+i;
            let val = Math.PI+i;

            buf.setFloat64(offs + offset, val, true);
            assertEq(ins.exports["get_f64_" + offset](offs), val);

            ins.exports["set_f64_" + offset](offs + 32, val);
            assertEq(buf.getFloat64(offs + 32 + offset, true), val);
        }

        for ( let i=0; i < 9; i++ ) {
            let offs = 512+i;
            let val = Math.fround(Math.PI+i);

            buf.setFloat32(offs + offset, val, true);
            assertEq(ins.exports["get_f32_" + offset](offs), val);

            ins.exports["set_f32_" + offset](offs + 32, val);
            assertEq(buf.getFloat32(offs + 32 + offset, true), val);
        }
    }
}
