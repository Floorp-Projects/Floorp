// |jit-test| skip-if: !wasmMemoryControlEnabled(); test-also=--wasm-memory64; test-also=--no-wasm-memory64

// This tests memory.discard and WebAssembly.Memory.discard() by placing data
// (the alphabet) halfway across a page boundary, then discarding the first
// page. The first half of the alphabet should be zeroed; the second should
// not. The memory should remain readable and writable.
//
// The ultimate goal is to release physical pages of memory back to the
// operating system, but we can't really observe memory metrics here. Oh well.

function initModule(discardOffset, discardLen, discardViaJS, shared, memType = 'i32') {
    const memProps = shared ? '4 4 shared' : '4'; // 4 pages
    const text = `(module
        (memory (export "memory") ${memType} ${memProps})
        (data "abcdefghijklmnopqrstuvwxyz")
        (func (export "init")
            ;; splat alphabet halfway across the 3/4 page boundary.
            ;; 196595 = (65536 * 3) - (26 / 2)
            (memory.init 0 (${memType}.const 196595) (i32.const 0) (i32.const 26))
        )
        (func (export "letter") (param i32) (result i32)
            (i32.load8_u (${memType}.add (${memType}.const 196595) ${memType == 'i64' ? '(i64.extend_i32_u (local.get 0))' : '(local.get 0)'}))
        )
        (func (export "discard")
            (memory.discard (${memType}.const ${discardOffset}) (${memType}.const ${discardLen}))
        )
    )`;
    const exp = wasmEvalText(text).exports;

    return [exp, discardViaJS ? () => exp.memory.discard(discardOffset, discardLen) : exp.discard];
}

function checkRegion(exp, min, max, expectLetters) {
    for (let i = min; i < max; i++) {
        const c = "a".charCodeAt(0) + i;
        const expected = expectLetters ? c : 0;
        assertEq(exp.letter(i), expected, `checking position of letter ${String.fromCharCode(c)}`);
    }
}
function checkFirstHalf(exp, expectLetters) { return checkRegion(exp, 0, 13, expectLetters) }
function checkSecondHalf(exp, expectLetters) { return checkRegion(exp, 13, 26, expectLetters) }
function checkWholeAlphabet(exp, expectLetters) { return checkRegion(exp, 0, 26, expectLetters) }

function testAll(func) {
    func(false, false, 'i32');
    func(false, true, 'i32');
    func(true, false, 'i32');
    func(true, true, 'i32');
    if (wasmMemory64Enabled()) {
        func(false, false, 'i64');
        func(false, true, 'i64');
        func(true, false, 'i64');
        func(true, true, 'i64');
    }
}

testAll(function testHappyPath(discardViaJS, shared, memType) {
    // Only page 3 of memory, half the alphabet
    const [exp, discard] = initModule(65536 * 2, 65536, discardViaJS, shared, memType);

    // All zero to start
    checkWholeAlphabet(exp, false);

    // Then all alphabet
    exp.init();
    checkWholeAlphabet(exp, true);

    // Discarding the first page clears the first half of the alphabet
    discard();
    checkFirstHalf(exp, false);
    checkSecondHalf(exp, true);

    // We should be able to write back to a discarded region of memory
    exp.init();
    checkWholeAlphabet(exp, true);

    // ...and then discard again
    discard();
    checkFirstHalf(exp, false);
    checkSecondHalf(exp, true);
});

testAll(function testZeroLen(discardViaJS, shared) {
    // Discard zero bytes
    const [exp, discard] = initModule(PageSizeInBytes * 2, 0, discardViaJS, shared);

    // Init the stuff
    exp.init();
    checkWholeAlphabet(exp, true);

    // Discarding zero bytes should be valid...
    discard();

    // ...but should have no effect.
    checkWholeAlphabet(exp, true);
});

testAll(function testWithGrow(discardViaJS, shared, memType) {
    if (shared) {
        return; // shared memories cannot grow
    }

    // Only page 3 of memory, half the alphabet. There is no max size on the
    // memory, so it will be subject to moving grows in 32-bit mode.
    const [exp, discard] = initModule(65536 * 2, 65536, discardViaJS, false, memType);

    // Start with the whole alphabet
    exp.init();
    checkWholeAlphabet(exp, true);

    // Discarding the first page clears the first half of the alphabet
    discard();
    checkFirstHalf(exp, false);
    checkSecondHalf(exp, true);

    // Oops we just grew by a preposterous amount, time to move
    exp.memory.grow(200)

    // The memory should still be readable
    checkFirstHalf(exp, false);
    checkSecondHalf(exp, true);

    // We should be able to write back to a discarded region of memory
    exp.init();
    checkWholeAlphabet(exp, true);

    // ...and then discard again
    discard();
    checkFirstHalf(exp, false);
    checkSecondHalf(exp, true);
});

testAll(function testOOB(discardViaJS, shared) {
    // Discard two pages where there is only one
    const [exp, discard] = initModule(PageSizeInBytes * 3, PageSizeInBytes * 2, discardViaJS, shared);

    exp.init();
    checkWholeAlphabet(exp, true);
    assertErrorMessage(() => discard(), WebAssembly.RuntimeError, /out of bounds/);

    // Ensure nothing was discarded
    checkWholeAlphabet(exp, true);
});

testAll(function testOOB2(discardViaJS, shared) {
    // Discard two pages starting near the end of 32-bit address space
    // (would trigger an overflow in 32-bit world)
    const [exp, discard] = initModule(2 ** 32 - PageSizeInBytes, PageSizeInBytes * 2, discardViaJS, shared);

    exp.init();
    checkWholeAlphabet(exp, true);
    assertErrorMessage(() => discard(), WebAssembly.RuntimeError, /out of bounds/);

    // Ensure nothing was discarded
    checkWholeAlphabet(exp, true);
});

testAll(function testOOB3(discardViaJS, shared) {
    // Discard nearly an entire 32-bit address space's worth of pages. Very exciting!
    const [exp, discard] = initModule(0, 2 ** 32 - PageSizeInBytes, discardViaJS, shared);

    exp.init();
    checkWholeAlphabet(exp, true);
    assertErrorMessage(() => discard(), WebAssembly.RuntimeError, /out of bounds/);

    // Ensure nothing was discarded
    checkWholeAlphabet(exp, true);
});

(function testOOB4() {
    // Discard an entire 32-bit address space's worth of pages. JS can do this
    // even to 32-bit memories because it can handle numbers bigger
    // than 2^32 - 1.
    const [exp, _] = initModule(0, 0, false); // pass zero to allow the wasm module to validate
    const discard = () => exp.memory.discard(0, 2 ** 32);

    exp.init();
    checkWholeAlphabet(exp, true);
    assertErrorMessage(() => discard(), WebAssembly.RuntimeError, /out of bounds/);

    // Ensure nothing was discarded
    checkWholeAlphabet(exp, true);
})();

if (wasmMemory64Enabled()) {
    (function testOverflow() {
        // Discard UINT64_MAX - (2 pages), starting from page 4. This overflows but puts both start and end in bounds.
        // This cannot be done with a JS discard because JS can't actually represent big enough integers.

        // The big ol' number here is 2^64 - (65536 * 2)
        const [exp, discard] = initModule(65536 * 3, `18_446_744_073_709_420_544`, false, false, 'i64');

        // Init the stuff
        exp.init();
        checkWholeAlphabet(exp, true);

        // Discarding should not be valid when it goes out of bounds
        assertErrorMessage(() => discard(), WebAssembly.RuntimeError, /out of bounds/);

        // Ensure nothing was discarded
        checkWholeAlphabet(exp, true);
    })();
}

testAll(function testMisalignedStart(discardViaJS, shared) {
    // Discard only the first half of the alphabet (this misaligns the start)
    const [exp, discard] = initModule(PageSizeInBytes * 3 - 13, 13, discardViaJS, shared);

    exp.init();
    checkWholeAlphabet(exp, true);
    assertErrorMessage(() => discard(), WebAssembly.RuntimeError, /unaligned/);

    // Ensure nothing was discarded
    checkWholeAlphabet(exp, true);
});

testAll(function testMisalignedEnd(discardViaJS, shared) {
    // Discard only the second half of the alphabet (this misaligns the end)
    const [exp, discard] = initModule(PageSizeInBytes * 3, 13, discardViaJS, shared);

    exp.init();
    checkWholeAlphabet(exp, true);
    assertErrorMessage(() => discard(), WebAssembly.RuntimeError, /unaligned/);

    // Ensure nothing was discarded
    checkWholeAlphabet(exp, true);
});
