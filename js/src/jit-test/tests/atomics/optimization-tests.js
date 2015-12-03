// Some optimization tests for the Atomics primitives.
//
// These do not test atomicity, just code generation on a single
// thread.
//
// It's useful to look at the code generated for this test with -D to
// the JS shell.
//
// Bug 1138348 - unconstrained use of byte registers on x64
// Bug 1077014 - code generation for Atomic operations for effect,
//               all platforms
// Bug 1141121 - immediate operand in atomic operations on x86/x64

if (!(this.Atomics && this.SharedArrayBuffer))
    quit(0);

var sum = 0;

function f(ia, k) {
    // For effect, variable value.  The generated code on x86/x64
    // should be one LOCK ADDB.  (On ARM, there should be no
    // sign-extend of the current value in the cell, otherwise this is
    // still a LDREX/STREX loop.)
    Atomics.add(ia, 0, k);

    // Ditto constant value.  Here the LOCK ADDB should have an
    // immediate operand.
    Atomics.add(ia, 0, 1);
}

function f2(ia, k) {
    // For effect, variable value and constant value.  The generated
    // code on x86/x64 should be one LOCK SUBB.
    Atomics.sub(ia, 2, k);

    // Ditto constant value.  Here the LOCK SUBB should have an
    // immediate operand.
    Atomics.sub(ia, 2, 1);
}

function f4(ia, k) {
    // For effect, variable value.  The generated code on x86/x64
    // should be one LOCK ORB.  (On ARM, there should be no
    // sign-extend of the current value in the cell, otherwise this is
    // still a LDREX/STREX loop.)
    Atomics.or(ia, 6, k);

    // Ditto constant value.  Here the LOCK ORB should have an
    // immediate operand.
    Atomics.or(ia, 6, 1);
}

function g(ia, k) {
    // For its value, variable value.  The generated code on x86/x64
    // should be one LOCK XADDB.
    sum += Atomics.add(ia, 1, k);

    // Ditto constant value.  XADD does not admit an immediate
    // operand, so in the second case there should be a preliminary
    // MOV of the immediate to the output register.
    sum += Atomics.add(ia, 1, 1);
}

function g2(ia, k) {
    // For its value, variable value.  The generated code on x86/x64
    // should be one LOCK XADDB, preceded by a NEG into the output
    // register instead of a MOV.
    sum += Atomics.sub(ia, 3, k);

    // Ditto constant value.  XADD does not admit an immediate
    // operand, so in the second case there should be a preliminary
    // MOV of the negated immediate to the output register.
    sum += Atomics.sub(ia, 3, 1);
}

function g4(ia, k) {
    // For its value, variable value.  The generated code on x86/x64
    // should be a loop around ORB ; CMPXCHGB
    sum += Atomics.or(ia, 7, k);

    // Ditto constant value.  Here the ORB in the loop should have
    // an immediate operand.
    sum += Atomics.or(ia, 7, 1);
}

function mod(stdlib, ffi, heap) {
    "use asm";

    var i8a = new stdlib.Int8Array(heap);
    var add = stdlib.Atomics.add;
    var sum = 0;

    function f3(k) {
	k = k|0;
	add(i8a, 4, 1);
	add(i8a, 4, k);
    }

    function g3(k) {
	k = k|0;
	sum = sum + add(i8a, 5, k)|0;
	sum = sum + add(i8a, 5, 1)|0;
    }

    return {f3:f3, g3:g3};
}

var i8a = new Int8Array(new SharedArrayBuffer(65536));
var { f3, g3 } = mod(this, {}, i8a.buffer);
for ( var i=0 ; i < 10000 ; i++ ) {
    f(i8a, i % 10);
    g(i8a, i % 10);
    f2(i8a, i % 10);
    g2(i8a, i % 10);
    f3(i % 10);
    g3(i % 10);
    f4(i8a, i % 10);
    g4(i8a, i % 10);
}

assertEq(i8a[0], ((10000 + 10000*4.5) << 24) >> 24);
assertEq(i8a[1], ((10000 + 10000*4.5) << 24) >> 24);
assertEq(i8a[2], ((-10000 + -10000*4.5) << 24) >> 24);
assertEq(i8a[3], ((-10000 + -10000*4.5) << 24) >> 24);
assertEq(i8a[4], ((10000 + 10000*4.5) << 24) >> 24);
assertEq(i8a[5], ((10000 + 10000*4.5) << 24) >> 24);
assertEq(i8a[6], 15);
assertEq(i8a[7], 15);
