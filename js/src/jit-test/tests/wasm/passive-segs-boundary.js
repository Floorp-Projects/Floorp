// |jit-test| skip-if: !wasmBulkMemSupported()

// Perform a test which,
//
// * if errKind is defined, is expected to fail with an exception
//   characterised by errKind and errText.
//
// * if errKind is undefined, is expected to succeed, in which case errKind
//   and errText are ignored.
//
// The function body will be [insn1, insn2] and is constructed according to
// four booleans:
//
// * isMem controls whether the module is constructed with memory or
//   table initializers.
//
// * haveStorage determines whether there is actually a memory or table to
//   work with.
//
// * haveInitA controls whether active initializers are added.
//
// * haveInitP controls whether passive initializers are added.

function do_test(insn1, insn2, errKind, errText,
                 isMem, haveStorage, haveInitA, haveInitP)
{
    let preamble;
    if (isMem) {
        let mem_def  = haveStorage ? "(memory 1 1)" : "";
        let mem_ia1  = `(data (i32.const 2) "\\03\\01\\04\\01")`;
        let mem_ia2  = `(data (i32.const 12) "\\07\\05\\02\\03\\06")`;
        let mem_ip1  = `(data passive "\\02\\07\\01\\08")`;
        let mem_ip2  = `(data passive "\\05\\09\\02\\07\\06")`;
        let mem_init = ``;
        if (haveInitA && haveInitP)
            mem_init = `${mem_ia1} ${mem_ip1} ${mem_ia2} ${mem_ip2}`;
        else if (haveInitA && !haveInitP) mem_init = `${mem_ia1} ${mem_ia2}`;
        else if (!haveInitA && haveInitP) mem_init = `${mem_ip1} ${mem_ip2}`;
        preamble
            = `;; -------- Memories --------
               ${mem_def}
               ;; -------- Memory initialisers --------
               ${mem_init}
              `;
    } else {
        let tab_def  = haveStorage ? "(table 30 30 funcref)" : "";
        let tab_ia1  = `(elem (i32.const 2) 3 1 4 1)`;
        let tab_ia2  = `(elem (i32.const 12) 7 5 2 3 6)`;
        let tab_ip1  = `(elem passive 2 7 1 8)`;
        let tab_ip2  = `(elem passive 5 9 2 7 6)`;
        let tab_init = ``;
        if (haveInitA && haveInitP)
            tab_init = `${tab_ia1} ${tab_ip1} ${tab_ia2} ${tab_ip2}`;
        else if (haveInitA && !haveInitP) tab_init = `${tab_ia1} ${tab_ia2}`;
        else if (!haveInitA && haveInitP) tab_init = `${tab_ip1} ${tab_ip2}`;
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


// Some handy specialisations of do_test().

function mem_test(insn1, insn2, errKind, errText,
                  haveStorage=true, haveInitA=true, haveInitP=true) {
    do_test(insn1, insn2, errKind, errText,
            /*isMem=*/true, haveStorage, haveInitA, haveInitP);
}

function mem_test_nofail(insn1, insn2,
                         haveStorage=true, haveInitA=true, haveInitP=true) {
    do_test(insn1, insn2, undefined, undefined,
            /*isMem=*/true, haveStorage, haveInitA, haveInitP);
}

function tab_test(insn1, insn2, errKind, errText,
                  haveStorage=true, haveInitA=true, haveInitP=true) {
    do_test(insn1, insn2, errKind, errText,
            /*isMem=*/false, haveStorage, haveInitA, haveInitP);
}

function tab_test_nofail(insn1, insn2,
                         haveStorage=true, haveInitA=true, haveInitP=true) {
    do_test(insn1, insn2, undefined, undefined,
            /*isMem=*/false, haveStorage, haveInitA, haveInitP);
}


//---- memory.{drop,init,copy} -------------------------------------------------

// The tested semantics for memory.drop, in the case where there's no memory,
// are as follows.  table.drop is analogous.
//
// no memory, no data segments:
//    drop with any args -> fail OOB
//                          [because there's nothing to drop]
//
// no memory, data segments, at least one of which is active:
//    -> always fails, regardless of insns
//       [because active segments implicitly reference memory 0]
//
// no memory, data segments, all of which are passive:
//    drop, segment index is OOB -> fail OOB
//                                  [because it refers to non existent segment]
//
//    drop, segment index is IB -> OK

// drop with no memory and no data segments
mem_test("data.drop 0", "",
         WebAssembly.CompileError, /data.drop segment index out of range/,
         /*haveStorage=*/false, /*haveInitA=*/false, /*haveInitP=*/false);

// drop with no memory but with both passive and active segments, ix in range
// and refers to a passive segment
mem_test("data.drop 3", "",
         WebAssembly.CompileError,
         /active data segment requires a memory section/,
         /*haveStorage=*/false, /*haveInitA=*/true, /*haveInitP=*/true);

// drop with no memory but with passive segments only, ix out of range
mem_test("data.drop 2", "",
         WebAssembly.CompileError, /data.drop segment index out of range/,
         /*haveStorage=*/false, /*haveInitA=*/false, /*haveInitP=*/true);

// drop with no memory but with passive segments only, ix in range
mem_test_nofail("data.drop 1", "",
                /*haveStorage=*/false, /*haveInitA=*/false, /*haveInitP=*/true);


// init with no memory and no data segs
mem_test("(memory.init 1 (i32.const 1234) (i32.const 1) (i32.const 1))", "",
         WebAssembly.CompileError, /can't touch memory without memory/,
         /*haveStorage=*/false, /*haveInitA=*/false, /*haveInitP=*/false);

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

// init: seg ix is valid passive, zero len, but src offset out of bounds.
// At edge of segment is OK
mem_test("",
         "(memory.init 1 (i32.const 1234) (i32.const 4) (i32.const 0))");

// One past end of segment is not OK
mem_test("",
         "(memory.init 1 (i32.const 1234) (i32.const 5) (i32.const 0))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, zero len, but dst offset out of bounds.
// At edge of memory is OK.
mem_test("",
         "(memory.init 1 (i32.const 0x10000) (i32.const 2) (i32.const 0))");

// One past end of memory is not OK.
mem_test("",
         "(memory.init 1 (i32.const 0x10001) (i32.const 2) (i32.const 0))",
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

//
//---- table.{drop,init} --------------------------------------------------

// drop with no tables and no elem segments
tab_test("elem.drop 0", "",
         WebAssembly.CompileError,
         /element segment index out of range for elem.drop/,
         /*haveStorage=*/false, /*haveInitA=*/false, /*haveInitP=*/false);

// drop with no tables but with both passive and active segments, ix in range
// and refers to a passive segment
tab_test("elem.drop 3", "",
         WebAssembly.CompileError,
         /active elem segment requires a table section/,
         /*haveStorage=*/false, /*haveInitA=*/true, /*haveInitP=*/true);

// drop with no tables but with passive segments only, ix out of range
tab_test("elem.drop 2", "",
         WebAssembly.CompileError,
         /element segment index out of range for elem.drop/,
         /*haveStorage=*/false, /*haveInitA=*/false, /*haveInitP=*/true);

// drop with no tables but with passive segments only, ix in range
tab_test_nofail("elem.drop 1", "",
                /*haveStorage=*/false, /*haveInitA=*/false, /*haveInitP=*/true);


// init with no table
tab_test("(table.init 1 (i32.const 12) (i32.const 1) (i32.const 1))", "",
         WebAssembly.CompileError, /table index out of range/,
         /*haveStorage=*/false, /*haveInitA=*/false, /*haveInitP=*/false);

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

// init: seg ix is valid passive, zero len, but src offset out of bounds.
// At edge of segment is OK.
tab_test("",
         "(table.init 1 (i32.const 12) (i32.const 4) (i32.const 0))");

// One past edge of segment is not OK.
tab_test("",
         "(table.init 1 (i32.const 12) (i32.const 5) (i32.const 0))",
         WebAssembly.RuntimeError, /index out of bounds/);

// init: seg ix is valid passive, zero len, but dst offset out of bounds.
// At edge of table is OK.
tab_test("",
         "(table.init 1 (i32.const 30) (i32.const 2) (i32.const 0))");

// One past edge of table is not OK.
tab_test("",
         "(table.init 1 (i32.const 31) (i32.const 2) (i32.const 0))",
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

// copy: zero length with dst offset out of bounds.
// At edge of table is OK.
tab_test("(table.copy (i32.const 30) (i32.const 15) (i32.const 0))",
         "");

// One past edge of table is not OK.
tab_test("(table.copy (i32.const 31) (i32.const 15) (i32.const 0))",
         "",
         WebAssembly.RuntimeError, /index out of bounds/);

// copy: zero length with src offset out of bounds
// At edge of table is OK.
tab_test("(table.copy (i32.const 15) (i32.const 30) (i32.const 0))",
         "");

// One past edge of table is not OK.
tab_test("(table.copy (i32.const 15) (i32.const 31) (i32.const 0))",
         "",
         WebAssembly.RuntimeError, /index out of bounds/);
