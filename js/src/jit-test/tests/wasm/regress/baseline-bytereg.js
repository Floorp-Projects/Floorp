// Bug 1322450 is about the baseline compiler not properly handling a byte store
// from a wider datum on 32-bit x86 because it does not move the value to be
// stored to a valid byte register if it is in a 32-bit register that does not
// have a byte part (EDI/ESI/EBP).
//
// This test is white-box because it knows about the register allocation order:
// the two values pushed onto the stack occupy EAX and ECX, and the i64.store8
// will use EDX for the index and (EDI or ESI or EBP) for the low register of
// the value to be stored.  The i64.store8 instruction will then assert in the
// assembler.
//
// If the baseline compiler starts allocating registers in a different order
// then this test will be ineffective.

wasmEvalText(`(module
    (memory 1)
    (func $run (param i64) (param i32) (param i32)
        get_local 1
        get_local 2
        i32.add

        get_local 1
        get_local 2
        i32.add

        i32.const 0
        get_local 0
        i64.store8

        drop
        drop
    )
    (export "run" $run)
)`);
