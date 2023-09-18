// In order not to run afoul of C++ UB we have our own non-C++ definitions of
// operations (they are actually jitted) that can operate racily on shared
// memory, see jit/shared/AtomicOperations-shared-jit.cpp.
//
// Operations on fixed-width 1, 2, 4, and 8 byte data are adequately tested
// elsewhere.  Here we specifically test our safe-when-racy replacements of
// memcpy and memmove.
//
// There are two primitives in the engine, memcpy_down and memcpy_up.  These are
// equivalent except when data overlap, in which case memcpy_down handles
// overlapping copies that move from higher to lower addresses and memcpy_up
// handles ditto from lower to higher.  memcpy uses memcpy_down always while
// memmove selects the one to use dynamically based on its arguments.

// Basic memcpy algorithm to be tested:
//
// - if src and target have the same alignment
//   - byte copy up to word alignment
//   - block copy as much as possible
//   - word copy as much as possible
//   - byte copy any tail
// - else if on a platform that can deal with unaligned access
//   (ie, x86, ARM64, and ARM if the proper flag is set)
//   - block copy as much as possible
//   - word copy as much as possible
//   - byte copy any tail
// - else // on a platform that can't deal with unaligned access
//   (ie ARM without the flag or x86 DEBUG builds with the
//   JS_NO_UNALIGNED_MEMCPY env var)
//   - block copy with byte copies
//   - word copy with byte copies
//   - byte copy any tail

var target_buf = new SharedArrayBuffer(1024);
var src_buf = new SharedArrayBuffer(1024);

///////////////////////////////////////////////////////////////////////////
//
// Different src and target buffer, this is memcpy "move down".  The same
// code is used in the engine for overlapping buffers when target addresses
// are lower than source addresses.

fill(src_buf);

// Basic 1K perfectly aligned copy, copies blocks only.
{
    let target = new Uint8Array(target_buf);
    let src = new Uint8Array(src_buf);
    clear(target_buf);
    target.set(src);
    check(target_buf, 0, 1024, 0);
}

// Buffers are equally aligned but not on a word boundary and not ending on a
// word boundary either, so this will copy first some bytes, then some blocks,
// then some words, and then some bytes.
{
    let fill = 0x79;
    clear(target_buf, fill);
    let target = new Uint8Array(target_buf, 1, 1022);
    let src = new Uint8Array(src_buf, 1, 1022);
    target.set(src);
    check_fill(target_buf, 0, 1, fill);
    check(target_buf, 1, 1023, 1);
    check_fill(target_buf, 1023, 1024, fill);
}

// Buffers are unequally aligned, we'll copy bytes only on some platforms and
// unaligned blocks/words on others.
{
    clear(target_buf);
    let target = new Uint8Array(target_buf, 0, 1023);
    let src = new Uint8Array(src_buf, 1);
    target.set(src);
    check(target_buf, 0, 1023, 1);
    check_zero(target_buf, 1023, 1024);
}

///////////////////////////////////////////////////////////////////////////
//
// Overlapping src and target buffer and the target addresses are always
// higher than the source addresses, this is memcpy "move up"

// Buffers are equally aligned but not on a word boundary and not ending on a
// word boundary either, so this will copy first some bytes, then some blocks,
// then some words, and then some bytes.
{
    fill(target_buf);
    let target = new Uint8Array(target_buf, 9, 999);
    let src = new Uint8Array(target_buf, 1, 999);
    target.set(src);
    check(target_buf, 9, 1008, 1);
    check(target_buf, 1008, 1024, 1008 & 255);
}

// Buffers are unequally aligned, we'll copy bytes only on some platforms and
// unaligned blocks/words on others.
{
    fill(target_buf);
    let target = new Uint8Array(target_buf, 2, 1022);
    let src = new Uint8Array(target_buf, 1, 1022);
    target.set(src);
    check(target_buf, 2, 1024, 1);
}

///////////////////////////////////////////////////////////////////////////
//
// Copy 0 to 127 bytes from and to a variety of addresses to check that we
// handle limits properly in these edge cases.

// Too slow in debug-noopt builds but we don't want to flag the test as slow,
// since that means it'll never be run.

if (this.getBuildConfiguration && !getBuildConfiguration("debug"))
{
    let t = new Uint8Array(target_buf);
    for (let my_src_buf of [src_buf, target_buf]) {
        for (let size=0; size < 127; size++) {
            for (let src_offs=0; src_offs < 8; src_offs++) {
                for (let target_offs=0; target_offs < 8; target_offs++) {
                    clear(target_buf, Math.random()*255);
                    let target = new Uint8Array(target_buf, target_offs, size);

                    // Zero is boring
                    let bias = (Math.random() * 100 % 12) | 0;

                    // Note src may overlap target partially
                    let src = new Uint8Array(my_src_buf, src_offs, size);
                    for ( let i=0; i < size; i++ )
                        src[i] = i+bias;

                    // We expect these values to be unchanged by the copy
                    let below = target_offs > 0 ? t[target_offs - 1] : 0;
                    let above = t[target_offs + size];

                    // Copy
                    target.set(src);

                    // Verify
                    check(target_buf, target_offs, target_offs + size, bias);
                    if (target_offs > 0)
                        assertEq(t[target_offs-1], below);
                    assertEq(t[target_offs+size], above);
                }
            }
        }
    }
}


// Utilities

function clear(buf, fill) {
    let a = new Uint8Array(buf);
    for ( let i=0; i < a.length; i++ )
        a[i] = fill;
}

function fill(buf) {
    let a = new Uint8Array(buf);
    for ( let i=0; i < a.length; i++ )
        a[i] = i & 255
}

function check(buf, from, to, startingWith) {
    let a = new Uint8Array(buf);
    for ( let i=from; i < to; i++ ) {
        assertEq(a[i], startingWith);
        startingWith = (startingWith + 1) & 255;
    }
}

function check_zero(buf, from, to) {
    check_fill(buf, from, to, 0);
}

function check_fill(buf, from, to, fill) {
    let a = new Uint8Array(buf);
    for ( let i=from; i < to; i++ )
        assertEq(a[i], fill);
}
