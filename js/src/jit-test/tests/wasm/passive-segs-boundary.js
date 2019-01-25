// |jit-test| skip-if: !wasmBulkMemSupported()

// Perform a test which,
//
// * if errKind is defined, is expected to fail with an exception
//   characterised by errKind and errText.
//
// * if errKind is undefined, is expected to succeed, in which case errKind
//   and errText are ignored.
//
// The function body will be [insn1, insn2].  isMem controls whether the
// module is constructed with memory or table initializers.  haveMemOrTable
// determines whether there is actually a memory or table to work with.

function do_test(insn1, insn2, errKind, errText, isMem, haveMemOrTable)
{
    let preamble;
    if (isMem) {
        let mem_def  = haveMemOrTable ? "(memory 1 1)" : "";
        let mem_init = haveMemOrTable
                       ? `(data (i32.const 2) "\\03\\01\\04\\01")
                          (data passive "\\02\\07\\01\\08")
                          (data (i32.const 12) "\\07\\05\\02\\03\\06")
                          (data passive "\\05\\09\\02\\07\\06")`
                       : "";
        preamble
            = `;; -------- Memories --------
               ${mem_def}
               ;; -------- Memory initialisers --------
               ${mem_init}
              `;
    } else {
        let tab_def  = haveMemOrTable ? "(table 30 30 anyfunc)" : "";
        let tab_init = haveMemOrTable
                       ? `(elem (i32.const 2) 3 1 4 1)
                          (elem passive 2 7 1 8)
                          (elem (i32.const 12) 7 5 2 3 6)
                          (elem passive 5 9 2 7 6)`
                       : "";
        preamble
            = `;; -------- Tables --------
               ${tab_def}
               ;; -------- Table initialisers --------
               ${tab_init}
               ;; ------ Functions (0..9) referred by the table/esegs ------
               (func (result i32) (i32.const 0))
               (func (result i32) (i32.const 1))
               (func (result i32) (i32.const 2))
               (func (result i32) (i32.const 3))
               (func (result i32) (i32.const 4))
               (func (result i32) (i32.const 5))
               (func (result i32) (i32.const 6))
               (func (result i32) (i32.const 7))
               (func (result i32) (i32.const 8))
               (func (result i32) (i32.const 9))
              `;
    }

    let txt = "(module\n" + preamble +
              `;; -------- testfn --------
               (func (export "testfn")
                 ${insn1}
                 ${insn2}
               )
               )`;

    if (!!errKind) {
        assertErrorMessage(
            () => {
                let inst = wasmEvalText(txt);
                inst.exports.testfn();
            },
            errKind,
            errText
        );
    } else {
        let inst = wasmEvalText(txt);
        assertEq(undefined, inst.exports.testfn());
    }
}

function mem_test(insn1, insn2, errKind, errText, haveMem=true) {
    do_test(insn1, insn2, errKind, errText,
            /*isMem=*/true, haveMem);
}

function mem_test_nofail(insn1, insn2) {
    do_test(insn1, insn2, undefined, undefined,
            /*isMem=*/true, /*haveMemOrTable=*/true);
}

function tab_test(insn1, insn2, errKind, errText, haveTab=true) {
    do_test(insn1, insn2, errKind, errText,
            /*isMem=*/false, haveTab);
}

function tab_test_nofail(insn1, insn2) {
    do_test(insn1, insn2, undefined, undefined,
            /*isMem=*/false, /*haveMemOrTable=*/true);
}


//---- memory.{drop,init,copy} -------------------------------------------------

// drop with no memory
mem_test("data.drop 3", "",
         WebAssembly.CompileError, /can't touch memory without memory/,
         false);

// init with no memory
mem_test("(memory.init 1 (i32.const 1234) (i32.const 1) (i32.const 1))", "",
         WebAssembly.CompileError, /can't touch memory without memory/,
         false);

// drop with data seg ix out of range
mem_test("data.drop 4", "",
         WebAssembly.CompileError, /data.drop segment index out of range/);

// init with data seg ix out of range
mem_test("(memory.init 4 (i32.const 1234) (i32.const 1) (i32.const 1))", "",
         WebAssembly.CompileError, /memory.init segment index out of range/);

// drop with data seg ix indicating an active segment
mem_test("data.drop 2", "",
         WebAssembly.RuntimeError, /use of dropped data segment/);

// init with data seg ix indicating an active segment
mem_test("(memory.init 2 (i32.const 1234) (i32.const 1) (i32.const 1))", "",
         WebAssembly.RuntimeError, /use of dropped data segment/);

// init, using a data seg ix more than once is OK
mem_test_nofail(
    "(memory.init 1 (i32.const 1234) (i32.const 1) (i32.const 1))",
    "(memory.init 1 (i32.const 4321) (i32.const 1) (i32.const 1))");

// drop, then drop
mem_test("data.drop 1",
         "data.drop 1",
         WebAssembly.RuntimeError, /use of dropped data segment/);

// drop, then init
mem_test("data.drop 1",
         "(memory.init 1 (i32.const 1234) (i32.const 1) (i32.const 1))",
         WebAssembly.RuntimeError, /use of dropped data segment/);

// init: seg ix is valid passive, but length to copy > len of seg
mem_test("",
         "(memory.init 1 (i32.const 1234) (i32.const 0) (i32.const 5))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, but implies copying beyond end of seg
mem_test("",
         "(memory.init 1 (i32.const 1234) (i32.const 2) (i32.const 3))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, but implies copying beyond end of dst
mem_test("",
         "(memory.init 1 (i32.const 0xFFFE) (i32.const 1) (i32.const 3))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, zero len, but src offset out of bounds
mem_test("",
         "(memory.init 1 (i32.const 1234) (i32.const 4) (i32.const 0))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, zero len, but dst offset out of bounds
mem_test("",
         "(memory.init 1 (i32.const 0x10000) (i32.const 2) (i32.const 0))",
         WebAssembly.RuntimeError, /index out of bounds/);

// drop: too many args
mem_test("data.drop 1 (i32.const 42)", "",
         WebAssembly.CompileError,
         /unused values not explicitly dropped by end of block/);

// init: too many args
mem_test("(memory.init 1 (i32.const 1) (i32.const 1) (i32.const 1) (i32.const 1))",
         "",
         SyntaxError, /parsing wasm text at/);

// init: too few args
mem_test("(memory.init 1 (i32.const 1) (i32.const 1))", "",
         WebAssembly.CompileError,
         /popping value from empty stack/);

// invalid argument types
{
    const tys  = ['i32', 'f32', 'i64', 'f64'];

    for (let ty1 of tys) {
    for (let ty2 of tys) {
    for (let ty3 of tys) {
        if (ty1 == 'i32' && ty2 == 'i32' && ty3 == 'i32')
            continue;  // this is the only valid case
        let i1 = `(memory.init 1 (${ty1}.const 1) (${ty2}.const 1) (${ty3}.const 1))`;
        mem_test(i1, "", WebAssembly.CompileError, /type mismatch/);
    }}}
}

const PAGESIZE = 65536;

// memory.fill: out of bounds, but should perform a partial fill.
//
// Arithmetic overflow of memory offset + len should not affect the behavior, we
// should still fill up to the limit.

function mem_fill(min, max, shared, backup, write=backup*2) {
    let ins = wasmEvalText(
        `(module
           (memory (export "mem") ${min} ${max} ${shared})
           (func (export "run") (param $offs i32) (param $val i32) (param $len i32)
             (memory.fill (get_local $offs) (get_local $val) (get_local $len))))`);
    // A fill past the end should throw *and* have filled all the way up to the end
    let offs = min*PAGESIZE - backup;
    let val = 37;
    assertErrorMessage(() => ins.exports.run(offs, val, write),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);
    let v = new Uint8Array(ins.exports.mem.buffer);
    for (let i=0; i < backup; i++)
        assertEq(v[offs+i], val);
    for (let i=0; i < offs; i++)
        assertEq(v[i], 0);
}

mem_fill(1, 1, "", 256);
mem_fill(1, 1, "", 257);
mem_fill(1, 1, "", 257, 0xFFFFFFFF); // offs + len overflows 32-bit

mem_fill(2, 4, "shared", 256);
mem_fill(2, 4, "shared", 257);
mem_fill(2, 4, "shared", 257, 0xFFFFFFFF); // offs + len overflows 32-bit

// memory.init: out of bounds of the memory or the segment, but should perform
// the operation up to the appropriate bound.
//
// Arithmetic overflow of memoffset + len or of bufferoffset + len should not
// affect the behavior.

// Note, the length of the data segment is 16.
const mem_init_len = 16;

function mem_init(min, max, shared, backup, write) {
    let ins = wasmEvalText(
        `(module
           (memory (export "mem") ${min} ${max} ${shared})
           (data passive "\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42\\42")
           (func (export "run") (param $offs i32) (param $len i32)
             (memory.init 0 (get_local $offs) (i32.const 0) (get_local $len))))`);
    // A fill writing past the end of the memory should throw *and* have filled
    // all the way up to the end.
    //
    // A fill reading past the end of the segment should throw *and* have filled
    // memory with as much data as was available.
    let offs = min*PAGESIZE - backup;
    assertErrorMessage(() => ins.exports.run(offs, write),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);
    let v = new Uint8Array(ins.exports.mem.buffer);
    for (let i=0; i < Math.min(backup, mem_init_len); i++)
        assertEq(v[offs + i], 0x42);
    for (let i=Math.min(backup, mem_init_len); i < backup; i++)
        assertEq(v[offs + i], 0);
    for (let i=0; i < offs; i++)
        assertEq(v[i], 0);
}

// We exceed the bounds of the memory but not of the data segment
mem_init(1, 1, "", mem_init_len/2, mem_init_len);
mem_init(1, 1, "", mem_init_len/2+1, mem_init_len);
mem_init(2, 4, "shared", mem_init_len/2, mem_init_len);
mem_init(2, 4, "shared", mem_init_len/2+1, mem_init_len);

// We exceed the bounds of the data segment but not the memory
mem_init(1, 1, "", mem_init_len*4, mem_init_len*2-2);
mem_init(1, 1, "", mem_init_len*4-1, mem_init_len*2-1);
mem_init(2, 4, "shared", mem_init_len*4, mem_init_len*2-2);
mem_init(2, 4, "shared", mem_init_len*4-1, mem_init_len*2-1);

// We arithmetically overflow the memory limit but not the segment limit
mem_init(1, "", "", mem_init_len/2, 0xFFFFFF00);

// We arithmetically overflow the segment limit but not the memory limit
mem_init(1, "", "", PAGESIZE, 0xFFFFFFFC);

// memory.copy: out of bounds of the memory for the source or target, but should
// perform the operation up to the appropriate bound.  Major cases:
//
// - non-overlapping regions
// - overlapping regions with src >= dest
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

function mem_copy(min, max, shared, srcOffs, targetOffs, len, copyDown=false) {
    let ins = wasmEvalText(
        `(module
           (memory (export "mem") ${min} ${max} ${shared})
           (func (export "run") (param $targetOffs i32) (param $srcOffs i32) (param $len i32)
             (memory.copy (get_local $targetOffs) (get_local $srcOffs) (get_local $len))))`);

    let v = new Uint8Array(ins.exports.mem.buffer);

    let targetAvail = v.length - targetOffs;
    let srcAvail = v.length - srcOffs;
    let targetLim = targetOffs + Math.min(len, targetAvail, srcAvail);
    let srcLim = srcOffs + Math.min(len, targetAvail, srcAvail);
    let immediateOOB = copyDown && (srcOffs + len > v.length || targetOffs + len > v.length);

    for (let i=srcOffs, j=0; i < srcLim; i++, j++)
        v[i] = j;
    assertErrorMessage(() => ins.exports.run(targetOffs, srcOffs, len),
                       WebAssembly.RuntimeError,
                       /index out of bounds/);

    // :lth wants lambda-lifting and closure optimizations
    var t = 0;
    var s = 0;
    var i = 0;
    function checkTarget() {
        if (i >= targetOffs && i < targetLim) {
            assertEq(v[i], (t++) & 0xFF);
            if (i >= srcOffs && i < srcLim)
                s++;
            return true;
        }
        return false;
    }
    function checkSource() {
        if (i >= srcOffs && i < srcLim) {
            assertEq(v[i], (s++) & 0xFF);
            if (i >= targetOffs && i < targetLim)
                t++;
            return true;
        }
        return false;
    }

    for (i=0; i < v.length; i++ ) {
        if (immediateOOB) {
            if (checkSource())
                continue;
        } else {
            if (copyDown && (checkSource() || checkTarget()))
                continue;
            if (!copyDown && (checkTarget() || checkSource()))
                continue;
        }
        assertEq(v[i], 0);
    }
}

// OOB target address, nonoverlapping
mem_copy(1, 1, "", 0, PAGESIZE-20, 40);
mem_copy(1, 1, "", 0, PAGESIZE-21, 39);
mem_copy(2, 4, "shared", 0, 2*PAGESIZE-20, 40);
mem_copy(2, 4, "shared", 0, 2*PAGESIZE-21, 39);

// OOB source address, nonoverlapping
mem_copy(1, 1, "", PAGESIZE-20, 0, 40);
mem_copy(1, 1, "", PAGESIZE-21, 0, 39);
mem_copy(2, 4, "shared", 2*PAGESIZE-20, 0, 40);
mem_copy(2, 4, "shared", 2*PAGESIZE-21, 0, 39);

// OOB target address, overlapping, src < target
mem_copy(1, 1, "", PAGESIZE-50, PAGESIZE-20, 40, true);

// OOB source address, overlapping, target < src
mem_copy(1, 1, "", PAGESIZE-20, PAGESIZE-50, 40);

// OOB both, overlapping, including target == src
mem_copy(1, 1, "", PAGESIZE-30, PAGESIZE-20, 40, true);
mem_copy(1, 1, "", PAGESIZE-20, PAGESIZE-30, 40);
mem_copy(1, 1, "", PAGESIZE-20, PAGESIZE-20, 40);

// Arithmetic overflow on source address.
mem_copy(1, "", "", PAGESIZE-20, 0, 0xFFFFF000);

// Arithmetic overflow on target adddress is an overlapping case.
mem_copy(1, 1, "", PAGESIZE-0x1000, PAGESIZE-20, 0xFFFFFF00, true);


//
//---- table.{drop,init} --------------------------------------------------

// drop with no table
tab_test("elem.drop 3", "",
         WebAssembly.CompileError, /can't elem.drop without a table/,
         false);

// init with no table
tab_test("(table.init 1 (i32.const 12) (i32.const 1) (i32.const 1))", "",
         WebAssembly.CompileError, /table index out of range/,
         false);

// drop with elem seg ix out of range
tab_test("elem.drop 4", "",
         WebAssembly.CompileError, /element segment index out of range for elem.drop/);

// init with elem seg ix out of range
tab_test("(table.init 4 (i32.const 12) (i32.const 1) (i32.const 1))", "",
         WebAssembly.CompileError, /table.init segment index out of range/);

// drop with elem seg ix indicating an active segment
tab_test("elem.drop 2", "",
         WebAssembly.RuntimeError, /use of dropped element segment/);

// init with elem seg ix indicating an active segment
tab_test("(table.init 2 (i32.const 12) (i32.const 1) (i32.const 1))", "",
         WebAssembly.RuntimeError, /use of dropped element segment/);

// init, using an elem seg ix more than once is OK
tab_test_nofail(
    "(table.init 1 (i32.const 12) (i32.const 1) (i32.const 1))",
    "(table.init 1 (i32.const 21) (i32.const 1) (i32.const 1))");

// drop, then drop
tab_test("elem.drop 1",
         "elem.drop 1",
         WebAssembly.RuntimeError, /use of dropped element segment/);

// drop, then init
tab_test("elem.drop 1",
         "(table.init 1 (i32.const 12) (i32.const 1) (i32.const 1))",
         WebAssembly.RuntimeError, /use of dropped element segment/);

// init: seg ix is valid passive, but length to copy > len of seg
tab_test("",
         "(table.init 1 (i32.const 12) (i32.const 0) (i32.const 5))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, but implies copying beyond end of seg
tab_test("",
         "(table.init 1 (i32.const 12) (i32.const 2) (i32.const 3))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, but implies copying beyond end of dst
tab_test("",
         "(table.init 1 (i32.const 28) (i32.const 1) (i32.const 3))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, zero len, but src offset out of bounds
tab_test("",
         "(table.init 1 (i32.const 12) (i32.const 4) (i32.const 0))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, zero len, but dst offset out of bounds
tab_test("",
         "(table.init 1 (i32.const 30) (i32.const 2) (i32.const 0))",
         WebAssembly.RuntimeError, /index out of bounds/);

// drop: too many args
tab_test("elem.drop 1 (i32.const 42)", "",
         WebAssembly.CompileError,
         /unused values not explicitly dropped by end of block/);

// init: too many args
tab_test("(table.init 1 (i32.const 1) (i32.const 1) (i32.const 1) (i32.const 1))",
         "",
         SyntaxError, /parsing wasm text at/);

// init: too few args
tab_test("(table.init 1 (i32.const 1) (i32.const 1))", "",
         WebAssembly.CompileError,
         /popping value from empty stack/);

// invalid argument types
{
    const tys  = ['i32', 'f32', 'i64', 'f64'];

    const ops = ['table.init 1', 'table.copy'];
    for (let ty1 of tys) {
    for (let ty2 of tys) {
    for (let ty3 of tys) {
    for (let op of ops) {
        if (ty1 == 'i32' && ty2 == 'i32' && ty3 == 'i32')
            continue;  // this is the only valid case
        let i1 = `(${op} (${ty1}.const 1) (${ty2}.const 1) (${ty3}.const 1))`;
        tab_test(i1, "", WebAssembly.CompileError, /type mismatch/);
    }}}}
}


//---- table.copy ---------------------------------------------------------

// There are no immediates here, only 3 dynamic args.  So we're limited to
// runtime boundary checks.

// passive-segs-smoketest.js tests the normal, non-exception cases of
// table.copy.  Here we just test the boundary-failure cases.  The
// table's valid indices are 0 .. 29 inclusive.

// copy: dst range invalid
tab_test("(table.copy (i32.const 28) (i32.const 1) (i32.const 3))",
         "",
         WebAssembly.RuntimeError, /index out of bounds/);

// copy: dst wraparound end of 32 bit offset space
tab_test("(table.copy (i32.const 0xFFFFFFFE) (i32.const 1) (i32.const 2))",
         "",
         WebAssembly.RuntimeError, /index out of bounds/);

// copy: src range invalid
tab_test("(table.copy (i32.const 15) (i32.const 25) (i32.const 6))",
         "",
         WebAssembly.RuntimeError, /index out of bounds/);

// copy: src wraparound end of 32 bit offset space
tab_test("(table.copy (i32.const 15) (i32.const 0xFFFFFFFE) (i32.const 2))",
         "",
         WebAssembly.RuntimeError, /index out of bounds/);

// copy: zero length with both offsets in-bounds is OK
tab_test_nofail(
    "(table.copy (i32.const 15) (i32.const 25) (i32.const 0))",
    "");

// copy: zero length with dst offset out of bounds
tab_test("(table.copy (i32.const 30) (i32.const 15) (i32.const 0))",
         "",
         WebAssembly.RuntimeError, /index out of bounds/);

// copy: zero length with src offset out of bounds
tab_test("(table.copy (i32.const 15) (i32.const 30) (i32.const 0))",
         "",
         WebAssembly.RuntimeError, /index out of bounds/);
