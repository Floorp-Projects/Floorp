
// This file tests multi-value returns.  It creates a chain of wasm functions
//
//   fnStart -> fnMid0 -> fnMid1 -> fnMid2 -> fnMid3 -> fnEnd
//
// When run, fnStart creates 12 (or in the non-simd case, 8) random values, of
// various types.  It then passes them to fnMid0.  That reorders them and
// passes them on to fnMid1, etc, until they arrive at fnEnd.
//
// fnEnd makes a small and reversible change to each value.  It then reorders
// them and returns all of them.  The returned values get passed back along
// the chain, being randomly reordered at each step, until they arrive back at
// fnStart.
//
// fnStart backs out the changes made in fnEnd and checks that the resulting
// values are the same as the originals it created.  If they are not, the test
// has failed.
//
// If the test passes, we can be sure each value got passed along the chain
// and back again correctly, despite being in a probably different argument or
// return position each time (due to the reordering).  As a side effect, this
// test also is a pretty good test of argument passing.  The number of values
// (10) is chosen so as to be larger than the number of args that can be
// passed in regs on any target; hence it also tests the logic for passing
// args in regs vs memory too.
//
// The whole process (generate and run a test program) is repeated 120 times.
//
// Doing this requires some supporting functions to be defined in wasm, by
// `funcs_util` and `funcs_rng` (a random number generator).
//
// It is almost impossible to understand what the tests do by reading the JS
// below.  Reading the generated wasm is required.  Search for "if (0)" below.

// Some utility functions for use in the generated code.
function funcs_util(simdEnabled) {
let t =
(simdEnabled ?
`;; Create a v128 value from 2 i64 values
 (func $v128_from_i64HL (export "v128_from_i64HL")
                        (param $i64hi i64) (param $i64lo i64) (result v128)
   (local $vec v128)
   (local.set $vec (i64x2.replace_lane 0 (local.get $vec) (local.get $i64lo)))
   (local.set $vec (i64x2.replace_lane 1 (local.get $vec) (local.get $i64hi)))
   (local.get $vec)
 )
 ;; Split a v128 up into pieces.
 (func $v128hi (export "v128hi") (param $vec v128) (result i64)
   (return (i64x2.extract_lane 1 (local.get $vec)))
 )
 (func $v128lo (export "v128lo") (param $vec v128) (result i64)
   (return (i64x2.extract_lane 0 (local.get $vec)))
 )
 ;; Return an i32 value, which is 0 if the args are identical and 1 otherwise.
 (func $v128ne (export "v128ne")
               (param $vec1 v128) (param $vec2 v128) (result i32)
   (return (v128.any_true (v128.xor (local.get $vec1) (local.get $vec2))))
 )`
 : ``/* simd not enabled*/
) +
`;; Move an i32 value forwards and backwards.
 (func $step_i32 (export "step_i32") (param $n i32) (result i32)
   (return (i32.add (local.get $n) (i32.const 1337)))
 )
 (func $unstep_i32 (export "unstep_i32") (param $n i32) (result i32)
   (return (i32.sub (local.get $n) (i32.const 1337)))
 )
 ;; Move an i64 value forwards and backwards.
 (func $step_i64 (export "step_i64") (param $n i64) (result i64)
   (return (i64.add (local.get $n) (i64.const 4771)))
 )
 (func $unstep_i64 (export "unstep_i64") (param $n i64) (result i64)
   (return (i64.sub (local.get $n) (i64.const 4771)))
 )
 ;; Move a f32 value forwards and backwards.  This is a bit tricky because
 ;; we need to guarantee that the backwards move exactly cancels out the
 ;; forward move.  So we multiply/divide exactly by 2 on the basis that that
 ;; will change the exponent but not the mantissa, at least for normalised
 ;; numbers.
 (func $step_f32 (export "step_f32") (param $n f32) (result f32)
   (return (f32.mul (local.get $n) (f32.const 2.0)))
 )
 (func $unstep_f32 (export "unstep_f32") (param $n f32) (result f32)
   (return (f32.div (local.get $n) (f32.const 2.0)))
 )
 ;; Move a f64 value forwards and backwards.
 (func $step_f64 (export "step_f64") (param $n f64) (result f64)
   (return (f64.mul (local.get $n) (f64.const 4.0)))
 )
 (func $unstep_f64 (export "unstep_f64") (param $n f64) (result f64)
   (return (f64.div (local.get $n) (f64.const 4.0)))
 )`
+ (simdEnabled ?
`;; Move a v128 value forwards and backwards.
 (func $step_v128 (export "step_v128") (param $vec v128) (result v128)
   (return (call $v128_from_i64HL
                 (i64.add (call $v128hi (local.get $vec)) (i64.const 1234))
                 (i64.add (call $v128lo (local.get $vec)) (i64.const 4321))
   ))
 )
 (func $unstep_v128 (export "unstep_v128") (param $vec v128) (result v128)
   (return (call $v128_from_i64HL
                 (i64.sub (call $v128hi (local.get $vec)) (i64.const 1234))
                 (i64.sub (call $v128lo (local.get $vec)) (i64.const 4321))
   ))
 )`
 : ``/* simd not enabled*/
);
return t;
}

// A Pseudo-RNG based on the C standard.  The core function generates only 16
// random bits.  We have to use it twice to generate a 32-bit random number
// and four times for a 64-bit random number.
let decls_rng =
`;; The RNG's state
 (global $rand_state
   (mut i32) (i32.const 1)
 )`;
function funcs_rng(simdEnabled) {
let t =
`;; Set the seed
 (func $rand_setSeed (param $seed i32)
   (global.set $rand_state (local.get $seed))
 )
 ;; Generate a 16-bit random number
 (func $rand_i16 (export "rand_i16") (result i32)
   (local $t i32)
   ;; update $rand_state
   (local.set $t (global.get $rand_state))
   (local.set $t (i32.mul (local.get $t) (i32.const 1103515245)))
   (local.set $t (i32.add (local.get $t) (i32.const 12345)))
   (global.set $rand_state (local.get $t))
   ;; pull 16 random bits out of it
   (local.set $t (i32.shr_u (local.get $t) (i32.const 15)))
   (local.set $t (i32.and (local.get $t) (i32.const 0xFFFF)))
   (local.get $t)
 )
 ;; Generate a 32-bit random number
 (func $rand_i32 (export "rand_i32") (result i32)
   (local $t i32)
   (local.set $t (call $rand_i16))
   (local.set $t (i32.shl (local.get $t) (i32.const 16)))
   (local.set $t (i32.or  (local.get $t) (call $rand_i16)))
   (local.get $t)
 )
 ;; Generate a 64-bit random number
 (func $rand_i64 (export "rand_i64") (result i64)
   (local $t i64)
   (local.set $t (i64.extend_i32_u (call $rand_i16)))
   (local.set $t (i64.shl (local.get $t) (i64.const 16)))
   (local.set $t (i64.or  (local.get $t) (i64.extend_i32_u (call $rand_i16))))
   (local.set $t (i64.shl (local.get $t) (i64.const 16)))
   (local.set $t (i64.or  (local.get $t) (i64.extend_i32_u (call $rand_i16))))
   (local.set $t (i64.shl (local.get $t) (i64.const 16)))
   (local.set $t (i64.or  (local.get $t) (i64.extend_i32_u (call $rand_i16))))
   (local.get $t)
 )
 ;; Generate a 32-bit random float.  This is something of a kludge in as much
 ;; as it does it by converting a random signed-int32 to a float32, which
 ;; means that we don't get any NaNs, infinities, denorms, etc, but OTOH
 ;; there's somewhat less randomness then there would be if we had allowed
 ;; such denorms in.
 (func $rand_f32 (export "rand_f32") (result f32)
   (f32.convert_i32_s (call $rand_i32))
 )
 ;; And similarly for 64-bit random floats
 (func $rand_f64 (export "rand_f64") (result f64)
   (f64.convert_i64_s (call $rand_i64))
 )`
+ (simdEnabled ?
`;; Generate a random 128-bit vector.
 (func $rand_v128 (export "rand_v128") (result v128)
   (call $v128_from_i64HL (call $rand_i64) (call $rand_i64))
 )`
: ``/* simd not enabled*/
);
return t;
}

// Helpers for function generation
function strcmp(s1,s2) {
    if (s1 < s2) return -1;
    if (s1 > s2) return 1;
    return 0;
}

// This generates the last function in the chain.  It merely returns its
// arguments in a different order, but first applies the relevant `_step`
// function to each value.  This is the only place in the process where
// the passed/return values are modified.  Hence it gives us a way to be
// sure that the values made it all the way from the start function to the
// end of the chain (here) and back to the start function.  Back in the
// start function, we will apply the relevant `_unstep` function to each
// returned value, which should give the value that was sent up the chain
// originally.
//
// Here and below, the following naming scheme is used:
//
// * taIn  -- types of arguments that come in to this function
// * taOut -- types of arguments that this function passes
//            to the next in the chain
// * trOut -- types of results that this function returns
// * trIn  -- types of results that the next function in the chain
//            returns to this function
//
// Hence 'a' vs 'r' distinguishes argument from return types, and 'In' vs
// 'Out' distinguishes values coming "in" to the function from those going
// "out".  The 'a'/'r' naming scheme is also used in the generated wasm (text).
function genEnd(myFuncName, taIn, trOut) {
    assertEq(taIn.length, trOut.length);
    let params = taIn.map(pair => `(param $a${pair.name} ${pair.type})`)
                     .join(` `);
    let retTys = trOut.map(pair => pair.type).join(` `);
    let t =
        `(func $${myFuncName} (export "${myFuncName}") ` +
        `  ${params} (result ${retTys})\n` +
        trOut.map(pair =>
            `  (call $step_${pair.type} (local.get $a${pair.name}))`)
             .join(`\n`) + `\n` +
        `)`;
    return t;
}

// This generates an intermediate function in the chain.  It takes args as
// specified by `taIn`, rearranges them to match `taOut`, passes those to the
// next function in the chain.  From which it receives return values as
// described by `trIn`, which it rearranges to match `trOut`, and returns
// those.  Overall, then, it permutes values both in the calling direction and
// in the returning direction.
function genMiddle(myFuncName, nextFuncName, taIn, trOut, taOut, trIn) {
    assertEq(taIn.length, taOut.length);
    assertEq(taIn.length, trIn.length);
    assertEq(taIn.length, trOut.length);
    let params = taIn.map(pair => `(param $a${pair.name} ${pair.type})`)
                     .join(` `);
    let retTys = trOut.map(pair => pair.type).join(` `);
    let trInSorted = trIn.toSorted((p1,p2) => strcmp(p1.name,p2.name));
    let t =
        `(func $${myFuncName} (export "${myFuncName}") ` +
        `  ${params} (result ${retTys})\n` +
        // Declare locals
        trInSorted
          .map(pair => `  (local $r${pair.name} ${pair.type})`)
          .join(`\n`) + `\n` +
        // Make the call
        `  (call $${nextFuncName} ` +
        taOut.map(pair => `(local.get $a${pair.name})`).join(` `) + `)\n` +
        // Capture the returned values
        trIn.toReversed()
            .map(pair => `  (local.set $r${pair.name})`).join(`\n`) + `\n` +
        // Return
        `  (return ` + trOut.map(pair => `(local.get $r${pair.name})`)
                            .join (` `) + `)\n` +
        `)`;
    return t;
}

// This generates the first function in the chain.  It creates random values
// for the initial arguments, passes them to the next arg in the chain,
// receives results, and checks that the results are as expected.
// NOTE!  The generated function returns zero on success, non-zero on failure.
function genStart(myFuncName, nextFuncName, taOut, trIn) {
    assertEq(taOut.length, trIn.length);
    let taOutSorted = taOut.toSorted((p1,p2) => strcmp(p1.name,p2.name));
    let trInSorted = trIn.toSorted((p1,p2) => strcmp(p1.name,p2.name));
    // `taOutSorted` and `trInSorted` should be identical.
    assertEq(taOutSorted.length, trInSorted.length);
    for (let i = 0; i < taOutSorted.length; i++) {
        assertEq(taOutSorted[i].name, trInSorted[i].name);
        assertEq(taOutSorted[i].type, trInSorted[i].type);
    }
    let t =
        `(func $${myFuncName} (export "${myFuncName}") (result i32)\n` +
        // Declare locals
        taOutSorted
          .map(pair => `  (local $a${pair.name} ${pair.type})`)
          .join(`\n`) + `\n` +
        trInSorted
          .map(pair => `  (local $r${pair.name} ${pair.type})`)
          .join(`\n`) + `\n` +
        `  (local $anyNotEqual i32)\n` +
        // Set up the initial values to be fed up the chain of calls and back
        // down again.  We expect them to be the same when they finally arrive
        // back.  Note we re-initialise the (wasm-side) RNG even though this
        // isn't actually necessary.
        `  (call $rand_setSeed (i32.const 1))\n` +
        taOutSorted
          .map(pair => `  (local.set $a${pair.name} (call $rand_${pair.type}))`)
          .join(`\n`) + `\n` +
        // Actually make the call
        `  (call $${nextFuncName} ` +
          taOut.map(pair => `(local.get $a${pair.name})`).join(` `) + `)\n` +
        // Capture the returned values
        trIn.toReversed()
            .map(pair => `  (local.set $r${pair.name})`).join(`\n`) + `\n` +

        // For each returned value, apply the relevant `_unstep` function,
        // then compare it against the original.  It should be the same, so
        // accumulate any signs of difference in $anyNotEqual.  Since
        // `taOutSorted` and `trInSorted` are identical we can iterate over
        // either.
        taOutSorted
          .map(pair =>
               `  (local.set $anyNotEqual \n` +
               `    (i32.or (local.get $anyNotEqual)\n` +
               `     (` +
               // v128 doesn't have a suitable .ne operator, so call a helper fn
               (pair.type === `v128` ? `call $v128ne` : `${pair.type}.ne`) +
               ` (local.get $a${pair.name})` +
               ` (call $unstep_${pair.type} (local.get $r${pair.name})))))`
              )
          .join(`\n`) + `\n` +
        `  (return (local.get $anyNotEqual))\n` +
        `)`;
    return t;
}

// A pseudo-random number generator that is independent of the one baked into
// each wasm program generated.  This is for use in JS only.  It isn't great,
// but at least it starts from a fixed place, which Math.random doesn't.  This
// produces a function `rand4js`, which takes an argument `n` and produces an
// integer value in the range `0 .. n-1` inclusive.  `n` needs to be less than
// or equal to 2^21 for this to work at all, and it needs to be much less than
// 2^21 (say, no more than 2^14) in order to get a reasonably even
// distribution of the values generated.
let rand4js_txt =
`(module
   (global $rand4js_state (mut i32) (i32.const 1))
   (func $rand4js (export "rand4js") (param $maxPlus1 i32) (result i32)
     (local $t i32)
     ;; update $rand4js_state
     (local.set $t (global.get $rand4js_state))
     (local.set $t (i32.mul (local.get $t) (i32.const 1103515245)))
     (local.set $t (i32.add (local.get $t) (i32.const 12345)))
     (global.set $rand4js_state (local.get $t))
     ;; Note, the low order bits are not very random.  Hence we dump the
     ;; low-order 11 bits.  This leaves us with at best 21 usable bits.
     (local.set $t (i32.shr_u (local.get $t) (i32.const 11)))
     (i32.rem_u (local.get $t) (local.get $maxPlus1))
   )
)`;
let rand4js = new WebAssembly.Instance(
                  new WebAssembly.Module(wasmTextToBinary(rand4js_txt)))
              .exports.rand4js;

// Fisher-Yates scheme for generating random permutations of a sequence.
// Result is a new array containing the original items in a different order.
// Original is unchanged.
function toRandomPermutation(input) {
    let n = input.length;
    let result = input.slice();
    assertEq(result.length, n);
    if (n < 2) return result;
    for (let i = 0; i < n - 1; i++) {
        let j = i + rand4js(n - i);
        let t = result[i];
        result[i] = result[j];
        result[j] = t;
    }
    return result;
}

// Top level test runner
function testMain(numIters) {
    // Check whether we can use SIMD.
    let simdEnabled = wasmSimdEnabled();

    // Names tagged with types.  This is set up to provide 10 values that
    // potentially can be passed in integer registers (5 x i32, 5 x i64) and
    // 10 values that potentially can be passed in FP/SIMD registers (3 x f32,
    // 3 x f64, 4 x v128).  This should cover both sides of the
    // arg-passed-in-reg/arg-passed-in-mem boundary for all of the primary
    // targets.
    let val0 = {name: "0", type: "i32"};
    let val1 = {name: "1", type: "i32"};
    let val2 = {name: "2", type: "i32"};
    let val3 = {name: "3", type: "i32"};
    let val4 = {name: "4", type: "i32"};

    let val5 = {name: "5", type: "i64"};
    let val6 = {name: "6", type: "i64"};
    let val7 = {name: "7", type: "i64"};
    let val8 = {name: "8", type: "i64"};
    let val9 = {name: "9", type: "i64"};

    let vala = {name: "a", type: "f32"};
    let valb = {name: "b", type: "f32"};
    let valc = {name: "c", type: "f32"};

    let vald = {name: "d", type: "f64"};
    let vale = {name: "e", type: "f64"};
    let valf = {name: "f", type: "f64"};

    let valg = {name: "g", type: "v128"};
    let valh = {name: "h", type: "v128"};
    let vali = {name: "i", type: "v128"};
    let valj = {name: "j", type: "v128"};

    // This is the base name/type vector,
    // of which we will create random permutations.
    let baseValVec;
    if (simdEnabled) {
        baseValVec
            = [val0, val1, val2, val3, val4, val5, val6, val7, val8, val9,
               vala, valb, valc, vald, vale, valf, valg, valh, vali, valj];
    } else {
        baseValVec
            = [val0, val1, val2, val3, val4, val5, val6, val7, val8, val9,
               vala, valb, valc, vald, vale, valf];
    }

    function summariseVec(valVec) {
        return valVec.map(pair => pair.name).join("");
    }

    print("\nsimdEnabled = " + simdEnabled + "\n");

    for (let testRun = 0; testRun < numIters; testRun++) {
        let tx0a = toRandomPermutation(baseValVec);
        let tx0r = toRandomPermutation(baseValVec);
        let tx1a = toRandomPermutation(baseValVec);
        let tx1r = toRandomPermutation(baseValVec);
        let tx2a = toRandomPermutation(baseValVec);
        let tx2r = toRandomPermutation(baseValVec);
        let tx3a = toRandomPermutation(baseValVec);
        let tx3r = toRandomPermutation(baseValVec);
        let tx4a = toRandomPermutation(baseValVec);
        let tx4r = toRandomPermutation(baseValVec);

        // Generate a 5-step chain of functions, each one passing and
        // returning different permutation of `baseValVec`.  The chain is:
        //   fnStart -> fnMid0 -> fnMid1 -> fnMid2 -> fnMid3 -> fnEnd
        let t_end   = genEnd("fnEnd", tx4a, tx4r);
        let t_mid3  = genMiddle("fnMid3", "fnEnd",  tx3a, tx3r, tx4a, tx4r);
        let t_mid2  = genMiddle("fnMid2", "fnMid3", tx2a, tx2r, tx3a, tx3r);
        let t_mid1  = genMiddle("fnMid1", "fnMid2", tx1a, tx1r, tx2a, tx2r);
        let t_mid0  = genMiddle("fnMid0", "fnMid1", tx0a, tx0r, tx1a, tx1r);
        let t_start = genStart("fnStart", "fnMid0", tx0a, tx0r);

        let txt = "(module (memory 1) " + "\n" +
            decls_rng + "\n" +
            funcs_util(simdEnabled) + "\n" + funcs_rng(simdEnabled) + "\n" +
            t_end + "\n" +
            t_mid3 + "\n" + t_mid2 + "\n" + t_mid1 + "\n" + t_mid0 + "\n" +
            t_start + "\n" +
            ")";

        if (0) print(txt);

        let mod = new WebAssembly.Module(wasmTextToBinary(txt));
        let ins = new WebAssembly.Instance(mod);
        let fns = ins.exports;

        // result == 0 means success, any other value means failure
        let result = fns.fnStart();
        if (/*failure*/result != 0 || (testRun % 120) == 0)
            print(" " + testRun + " " +
                  [tx0a,tx0r,tx1a,tx1r,tx2a,tx2r,tx3a,tx3r,tx4a,tx4r]
                  .map(e => summariseVec(e)).join("/") + "  "
                  + (result == 0 ? "pass" : "FAIL"));

        assertEq(result, 0);
    }
}

testMain(/*numIters=*/120);
