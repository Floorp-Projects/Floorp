// |jit-test| skip-if: !wasmBulkMemSupported()

// Sundry test cases for the "partial write" bounds checking semantics.

// table.init: out of bounds of the table or the element segment, but should
// perform the operation up to the appropriate bound.
//
// Arithmetic overflow of tableoffset + len or of segmentoffset + len should not
// affect the behavior.

// Note, the length of the element segment is 16.
const tbl_init_len = 16;

function tbl_init(min, max, backup, write, segoffs=0) {
    let ins = wasmEvalText(
        `(module
           (table (export "tbl") ${min} ${max} funcref)
           (elem passive $f0 $f1 $f2 $f3 $f4 $f5 $f6 $f7 $f8 $f9 $f10 $f11 $f12 $f13 $f14 $f15)
           (func $f0 (export "f0"))
           (func $f1 (export "f1"))
           (func $f2 (export "f2"))
           (func $f3 (export "f3"))
           (func $f4 (export "f4"))
           (func $f5 (export "f5"))
           (func $f6 (export "f6"))
           (func $f7 (export "f7"))
           (func $f8 (export "f8"))
           (func $f9 (export "f9"))
           (func $f10 (export "f10"))
           (func $f11 (export "f11"))
           (func $f12 (export "f12"))
           (func $f13 (export "f13"))
           (func $f14 (export "f14"))
           (func $f15 (export "f15"))
           (func (export "run") (param $offs i32) (param $len i32)
             (table.init 0 (local.get $offs) (i32.const ${segoffs}) (local.get $len))))`);
    // A fill writing past the end of the table should throw *and* have filled
    // all the way up to the end.
    //
    // A fill reading past the end of the segment should throw *and* have filled
    // table with as much data as was available.
    let offs = min - backup;
    assertErrorMessage(() => ins.exports.run(offs, write),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);
    let tbl = ins.exports.tbl;
    for (let i=0; i < Math.min(backup, tbl_init_len - segoffs); i++) {
        assertEq(tbl.get(offs + i), ins.exports["f" + (i + segoffs)]);
    }
    for (let i=Math.min(backup, tbl_init_len); i < backup; i++)
        assertEq(tbl.get(offs + i), null);
    for (let i=0; i < offs; i++)
        assertEq(tbl.get(i), null);
}

// We exceed the bounds of the table but not of the element segment
tbl_init(tbl_init_len*2, tbl_init_len*4, Math.floor(tbl_init_len/2), tbl_init_len);
tbl_init(tbl_init_len*2, tbl_init_len*4, Math.floor(tbl_init_len/2)-1, tbl_init_len);

// We exceed the bounds of the element segment but not the table
tbl_init(tbl_init_len*10, tbl_init_len*20, tbl_init_len*4, tbl_init_len*2);
tbl_init(tbl_init_len*10, tbl_init_len*20, tbl_init_len*4-1, tbl_init_len*2-1);

// We arithmetically overflow the table limit but not the segment limit
tbl_init(tbl_init_len*4, tbl_init_len*4, tbl_init_len, 0xFFFFFFF0);

// We arithmetically overflow the segment limit but not the table limit
tbl_init(tbl_init_len, tbl_init_len, tbl_init_len, 0xFFFFFFFC, Math.floor(tbl_init_len/2));

// table.copy: out of bounds of the table for the source or target, but should
// perform the operation up to the appropriate bound.  Major cases:
//
// - non-overlapping regions
// - overlapping regions with src > dest
// - overlapping regions with src == dest
// - overlapping regions with src < dest
// - arithmetic overflow on src addresses
// - arithmetic overflow on target addresses
//
// for each of those,
//
// - src address oob
// - target address oob
// - both oob
//
// Note we do not test the multi-table case here because that is part of the
// reftypes proposal; tests are in the gc/ subdirectory.

const tbl_copy_len = 16;

function tbl_copy(min, max, srcOffs, targetOffs, len, copyDown=false) {
    let ins = wasmEvalText(
        `(module
           (table (export "tbl") ${min} ${max} funcref)
           (func $f0 (export "f0"))
           (func $f1 (export "f1"))
           (func $f2 (export "f2"))
           (func $f3 (export "f3"))
           (func $f4 (export "f4"))
           (func $f5 (export "f5"))
           (func $f6 (export "f6"))
           (func $f7 (export "f7"))
           (func $f8 (export "f8"))
           (func $f9 (export "f9"))
           (func $f10 (export "f10"))
           (func $f11 (export "f11"))
           (func $f12 (export "f12"))
           (func $f13 (export "f13"))
           (func $f14 (export "f14"))
           (func $f15 (export "f15"))
           (func (export "run") (param $targetOffs i32) (param $srcOffs i32) (param $len i32)
             (table.copy (local.get $targetOffs) (local.get $srcOffs) (local.get $len))))`);

    let tbl = ins.exports.tbl;

    let targetAvail = tbl.length - targetOffs;
    let srcAvail = tbl.length - srcOffs;
    let targetLim = targetOffs + Math.min(len, targetAvail, srcAvail);
    let srcLim = srcOffs + Math.min(len, targetAvail, srcAvail);
    let immediateOOB = copyDown && (srcOffs + len > tbl.length || targetOffs + len > tbl.length);

    for (let i=srcOffs, j=0; i < srcLim; i++, j++)
        tbl.set(i, ins.exports["f" + j]);
    assertErrorMessage(() => ins.exports.run(targetOffs, srcOffs, len),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);

    var t = 0;
    var s = 0;
    var i = 0;
    function checkTarget() {
        if (i >= targetOffs && i < targetLim) {
            assertEq(tbl.get(i), ins.exports["f" + (t++)]);
            if (i >= srcOffs && i < srcLim)
                s++;
            return true;
        }
        return false;
    }
    function checkSource() {
        if (i >= srcOffs && i < srcLim) {
            assertEq(tbl.get(i), ins.exports["f" + (s++)]);
            if (i >= targetOffs && i < targetLim)
                t++;
            return true;
        }
        return false;
    }

    for (i=0; i < tbl.length; i++ ) {
        if (immediateOOB) {
            if (checkSource())
                continue;
        } else {
            if (copyDown && (checkSource() || checkTarget()))
                continue;
            if (!copyDown && (checkTarget() || checkSource()))
                continue;
        }
        assertEq(tbl.get(i), null);
    }
}

// OOB target address, nonoverlapping
tbl_copy(tbl_copy_len*2, tbl_copy_len*4, 0, Math.floor(1.5*tbl_copy_len), tbl_copy_len);
tbl_copy(tbl_copy_len*2, tbl_copy_len*4, 0, Math.floor(1.5*tbl_copy_len)-1, tbl_copy_len-1);

// OOB source address, nonoverlapping
tbl_copy(tbl_copy_len*2, tbl_copy_len*4, Math.floor(1.5*tbl_copy_len), 0, tbl_copy_len);
tbl_copy(tbl_copy_len*2, tbl_copy_len*4, Math.floor(1.5*tbl_copy_len)-1, 0, tbl_copy_len-1);

// OOB target address, overlapping, src < target
tbl_copy(tbl_copy_len*2, tbl_copy_len*4, tbl_copy_len-5, Math.floor(1.5*tbl_copy_len), tbl_copy_len, true);

// OOB source address, overlapping, target < src
tbl_copy(tbl_copy_len*2, tbl_copy_len*4, Math.floor(1.5*tbl_copy_len), tbl_copy_len-5, tbl_copy_len);

// OOB both, overlapping, including src == target
tbl_copy(tbl_copy_len*2, tbl_copy_len*4, tbl_copy_len+5, Math.floor(1.5*tbl_copy_len), tbl_copy_len, true);
tbl_copy(tbl_copy_len*2, tbl_copy_len*4, Math.floor(1.5*tbl_copy_len), tbl_copy_len+5, tbl_copy_len);
tbl_copy(tbl_copy_len*2, tbl_copy_len*4, tbl_copy_len+5, tbl_copy_len+5, tbl_copy_len);

// Arithmetic overflow on source address.
tbl_copy(tbl_copy_len*8, tbl_copy_len*8, tbl_copy_len*7, 0, 0xFFFFFFE0);

// Arithmetic overflow on target adddress is an overlapping case.
tbl_copy(tbl_copy_len*8, tbl_copy_len*8, 0, tbl_copy_len*7, 0xFFFFFFE0, true);
