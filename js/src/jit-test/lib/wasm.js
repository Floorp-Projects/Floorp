if (!wasmIsSupported())
    quit();

load(libdir + "asserts.js");

function canRunHugeMemoryTests() {
    // We're aiming for 64-bit desktop builds with no interesting analysis
    // running that might inflate memory consumption unreasonably.  It's OK if
    // they're debug builds, though.
    //
    // The build configuration object may be extended at any time with new
    // properties, so neither an allowlist of properties that can be true or a
    // blocklist of properties that can't be true is great.  But the latter is
    // probably better.
    let blocked = ['rooting-analysis','simulator',
                   'android','wasi','asan','tsan','ubsan','dtrace','valgrind'];
    for ( let b of blocked ) {
        if (getBuildConfiguration(b)) {
            print("Failing canRunHugeMemoryTests() because '" + b + "' is true");
            return false;
        }
    }
    if (getBuildConfiguration("pointer-byte-size") != 8) {
        print("Failing canRunHugeMemoryTests() because the build is not 64-bit");
        return false;
    }
    return true;
}

// On 64-bit systems with explicit bounds checking, ion and baseline can handle
// 65536 pages.

var PageSizeInBytes = 65536;
var MaxBytesIn32BitMemory = 0;
if (largeArrayBufferSupported()) {
    MaxBytesIn32BitMemory = 65536*PageSizeInBytes;
} else {
    // This is an overestimate twice: first, the max byte value is divisible by
    // the page size; second, it must be a valid bounds checking immediate.  But
    // INT32_MAX is fine for testing.
    MaxBytesIn32BitMemory = 0x7FFF_FFFF;
}
var MaxPagesIn32BitMemory = Math.floor(MaxBytesIn32BitMemory / PageSizeInBytes);

function wasmEvalText(str, imports) {
    let binary = wasmTextToBinary(str);
    let valid = WebAssembly.validate(binary);

    let m;
    try {
        m = new WebAssembly.Module(binary);
        assertEq(valid, true, "failed WebAssembly.validate but still compiled successfully");
    } catch(e) {
        if (!e.toString().match(/out of memory/)) {
            assertEq(valid, false, `passed WebAssembly.validate but failed to compile: ${e}`);
        }
        throw e;
    }

    return new WebAssembly.Instance(m, imports);
}

function wasmValidateText(str) {
    let binary = wasmTextToBinary(str);
    let valid = WebAssembly.validate(binary);
    if (!valid) {
        new WebAssembly.Module(binary);
        throw new Error("module failed WebAssembly.validate but compiled successfully");
    }
    assertEq(valid, true, "wasm module was invalid");
}

function wasmFailValidateText(str, pattern) {
    let binary = wasmTextToBinary(str);
    assertEq(WebAssembly.validate(binary), false, "module passed WebAssembly.validate when it should not have");
    assertErrorMessage(() => new WebAssembly.Module(binary), WebAssembly.CompileError, pattern, "module failed WebAssembly.validate but did not fail to compile as expected");
}

// Expected compilation failure can happen in a couple of ways:
//
// - The compiler can be available but not capable of recognizing some opcodes:
//   Compilation will start, but will fail with a CompileError.  This is what
//   happens without --wasm-gc if opcodes enabled by --wasm-gc are used.
//
// - The compiler can be unavailable: Compilation will not start at all but will
//   throw an Error.  This is what happens with "--wasm-gc --wasm-compiler=X" if
//   X does not support the features enabled by --wasm-gc.

function wasmCompilationShouldFail(bin, compile_error_regex) {
    try {
        new WebAssembly.Module(bin);
    } catch (e) {
        if (e instanceof WebAssembly.CompileError) {
            assertEq(compile_error_regex.test(e), true);
        } else if (e instanceof Error) {
            assertEq(/can't use wasm debug\/gc without baseline/.test(e), true);
        } else {
            throw new Error("Unexpected exception value:\n" + e);
        }
    }
}

function mismatchError(actual, expect) {
    var str = `(type mismatch: expression has type ${actual} but expected ${expect})|` +
              `(type mismatch: expected ${expect}, found ${actual}\)`;
    return RegExp(str);
}

const emptyStackError = /(from empty stack)|(nothing on stack)/;
const unusedValuesError = /(unused values not explicitly dropped by end of block)|(values remaining on stack at end of block)/;

function jsify(wasmVal) {
    if (wasmVal === 'nan')
        return NaN;
    if (wasmVal === 'inf')
        return Infinity;
    if (wasmVal === '-inf')
        return Infinity;
    if (wasmVal === '-0')
        return -0;
    return wasmVal;
}

function _augmentSrc(src, assertions) {
    let i = 0;
    let newSrc = src.substr(0, src.lastIndexOf(')'));
    for (let { func, args, expected, type } of assertions) {
        newSrc += `
        (func (export "assert_${i++}") (result i32)
         ${ args ? args.join('\n') : '' }
         call ${func}`;

        if (typeof expected !== 'undefined') {
            switch (type) {
                case 'f32':
                    newSrc += `
         i32.reinterpret_f32
         ${(function () {
             if (expected == 'nan:arithmetic') {
               expected = '0x7FC00000';
               return '(i32.const 0x7FC00000) i32.and';
             }
             return '';
         })()}
         i32.const ${expected}
         i32.eq`;
                    break;
                case 'f64':
                    newSrc += `
         i64.reinterpret_f64
         ${(function () {
             if (expected == 'nan:arithmetic') {
               expected = '0x7FF8000000000000';
               return '(i64.const 0x7FF8000000000000) i64.and';
             }
             return '';
         })()}
         i64.const ${expected}
         i64.eq`;
                    break;
                case 'i32':
                    newSrc += `
         i32.const ${expected}
         i32.eq`;
                    break;
                case 'i64':
                    newSrc += `
         i64.const ${expected}
         i64.eq`;
                    break;
                case 'v128':
                    newSrc += `
         v128.const ${expected}
         i8x16.eq
         i8x16.all_true`;
                    break;
                default:
                    throw new Error("unexpected usage of wasmAssert");
            }
        } else {
            // Always true when there's no expected return value.
            newSrc += "\ni32.const 1";
        }

        newSrc += ')\n';
    }
    newSrc += ')';
    return newSrc;
}

function wasmAssert(src, assertions, maybeImports = {}, exportBox = null) {
    let { exports } = wasmEvalText(_augmentSrc(src, assertions), maybeImports);
    if (exportBox !== null)
        exportBox.exports = exports;
    for (let i = 0; i < assertions.length; i++) {
        let { func, expected, params } = assertions[i];
        let paramText = params ? params.join(', ') : '';
        assertEq(exports[`assert_${i}`](), 1,
                 `Unexpected value when running ${func}(${paramText}), expecting ${expected}.`);
    }
}

// Fully test a module:
// - ensure it validates.
// - ensure it compiles and produces the expected result.
// - ensure textToBinary(binaryToText(binary)) = binary
// Preconditions:
// - the binary module must export a function called "run".
function wasmFullPass(text, expected, maybeImports, ...args) {
    let binary = wasmTextToBinary(text);
    assertEq(WebAssembly.validate(binary), true, "Must validate.");

    let module = new WebAssembly.Module(binary);
    let instance = new WebAssembly.Instance(module, maybeImports);
    assertEq(typeof instance.exports.run, 'function', "A 'run' function must be exported.");
    assertEq(instance.exports.run(...args), expected, "Initial module must return the expected result.");
}

// Ditto, but expects a function named '$run' instead of exported with this name.
function wasmFullPassI64(text, expected, maybeImports, ...args) {
    let binary = wasmTextToBinary(text);
    assertEq(WebAssembly.validate(binary), true, "Must validate.");

    let augmentedSrc = _augmentSrc(text, [ { type: 'i64', func: '$run', args, expected } ]);
    let augmentedBinary = wasmTextToBinary(augmentedSrc);

    let module = new WebAssembly.Module(augmentedBinary);
    let instance = new WebAssembly.Instance(module, maybeImports);
    assertEq(instance.exports.assert_0(), 1);
}

function wasmRunWithDebugger(wast, lib, init, done) {
    let g = newGlobal({newCompartment: true});
    let dbg = new Debugger(g);

    g.eval(`
var wasm = wasmTextToBinary(\`${wast}\`);
var lib = ${lib || 'undefined'};
var m = new WebAssembly.Instance(new WebAssembly.Module(wasm), lib);`);

    var wasmScript = dbg.findScripts().filter(s => s.format == 'wasm')[0];

    init({dbg, wasmScript, g,});
    let result = undefined, error = undefined;
    try {
        result = g.eval("m.exports.test()");
    } catch (ex) {
        error = ex;
    }
    done({dbg, result, error, wasmScript, g,});
}

const WasmHelpers = {};

(function() {
    let enabled = false;
    try {
        enableSingleStepProfiling();
        disableSingleStepProfiling();
        enabled = true;
    } catch (e) {}
    WasmHelpers.isSingleStepProfilingEnabled = enabled;
})();

// The cache of matched and unmatched strings seriously speeds up matching on
// the emulators and makes tests time out less often.

var matched = {};
var unmatched = {};

WasmHelpers._normalizeStack = (stack, preciseStacks) => {
    var wasmFrameTypes = [
        {re:/^jit call to int64(?: or v128)? wasm function$/,             sub:"i64>"},
        {re:/^out-of-line coercion for jit entry arguments \(in wasm\)$/, sub:"ool>"},
        {re:/^wasm-function\[(\d+)\] \(.*\)$/,                            sub:"$1"},
        {re:/^(fast|slow) exit trampoline (?:to native )?\(in wasm\)$/,   sub:"<"},
        {re:/^call to(?: asm.js)? native (.*) \(in wasm\)$/,              sub:"$1"},
        {re:/ \(in wasm\)$/,                                              sub:""}
    ];

    let entryRegexps;
    if (preciseStacks) {
        entryRegexps = [
            {re:/^slow entry trampoline \(in wasm\)$/,                    sub:"!>"},
            {re:/^fast entry trampoline \(in wasm\)$/,                    sub:">"},
        ];
    } else {
        entryRegexps = [
            {re:/^(fast|slow) entry trampoline \(in wasm\)$/,             sub:">"}
        ];
    }
    wasmFrameTypes = entryRegexps.concat(wasmFrameTypes);

    var framesIn = stack.split(',');
    var framesOut = [];
  outer:
    for (let frame of framesIn) {
        if (unmatched[frame])
            continue;
        let probe = matched[frame];
        if (probe !== undefined) {
            framesOut.push(probe);
            continue;
        }
        for (let {re, sub} of wasmFrameTypes) {
            if (re.test(frame)) {
                let repr = frame.replace(re, sub);
                framesOut.push(repr);
                matched[frame] = repr;
                continue outer;
            }
        }
        unmatched[frame] = true;
    }

    return framesOut.join(',');
};

WasmHelpers._removeAdjacentDuplicates = array => {
    if (array.length < 2)
        return;
    let i = 0;
    for (let j = 1; j < array.length; j++) {
        if (array[i] !== array[j])
            array[++i] = array[j];
    }
    array.length = i + 1;
}

WasmHelpers.normalizeStacks = (stacks, preciseStacks = false) => {
    let observed = [];
    for (let i = 0; i < stacks.length; i++)
        observed[i] = WasmHelpers._normalizeStack(stacks[i], preciseStacks);
    WasmHelpers._removeAdjacentDuplicates(observed);
    return observed;
};

WasmHelpers._compareStacks = (got, expect) => {
    if (got.length != expect.length) {
        return false;
    }
    for (let i = 0; i < got.length; i++) {
        if (got[i] !== expect[i])
            return false;
    }
    return true;
}

WasmHelpers.assertEqImpreciseStacks = (got, expect) => {
    let observed = WasmHelpers.normalizeStacks(got, /* precise */ false);
    let same = WasmHelpers._compareStacks(observed, expect);
    if (!same) {
        if (observed.length != expect.length) {
            print(`Got:\n${observed.toSource()}\nExpect:\n${expect.toSource()}`);
            assertEq(observed.length, expect.length);
        }
        for (let i = 0; i < observed.length; i++) {
            if (observed[i] !== expect[i]) {
                print(`On stack ${i}, Got:\n${observed[i]}\nExpect:\n${expect[i]}`);
                assertEq(observed[i], expect[i]);
            }
        }
    }
}

WasmHelpers.extractStackFrameFunction = (frameString) => {
    var [_, name, filename, line, column] = frameString.match(/^(.*)@(.*):(.*):(.*)$/);
    if (name)
        return name;
    if (/wasm-function/.test(line))
        return line;
    return "";
};

WasmHelpers.assertStackTrace = (exception, expected) => {
    let callsites = exception.stack.trim().split('\n').map(WasmHelpers.extractStackFrameFunction);
    assertEq(callsites.length, expected.length);
    for (let i = 0; i < callsites.length; i++) {
        assertEq(callsites[i], expected[i]);
    }
};

WasmHelpers.nextLineNumber = (n=1) => {
    return +(new Error().stack).split('\n')[1].split(':')[1] + n;
}

WasmHelpers.startProfiling = () => {
    if (!WasmHelpers.isSingleStepProfilingEnabled)
        return;
    enableSingleStepProfiling();
}

WasmHelpers.endProfiling = () => {
    if (!WasmHelpers.isSingleStepProfilingEnabled)
        return;
    return disableSingleStepProfiling();
}

WasmHelpers.assertEqPreciseStacks = (observed, expectedStacks) => {
    if (!WasmHelpers.isSingleStepProfilingEnabled)
        return null;

    observed = WasmHelpers.normalizeStacks(observed, /* precise */ true);

    for (let i = 0; i < expectedStacks.length; i++) {
        if (WasmHelpers._compareStacks(observed, expectedStacks[i]))
            return i;
    }

    throw new Error(`no plausible stacks found, observed: ${observed.join('/')}
Expected one of:
${expectedStacks.map(stacks => stacks.join("/")).join('\n')}`);
}

function fuzzingSafe() {
    return typeof getErrorNotes == 'undefined';
}

// Common instantiations of wasm values for dynamic type check testing

// Valid values for funcref
let WasmFuncrefValues = [
    wasmEvalText(`(module (func (export "")))`).exports[''],
];

// Valid values for structref/arrayref
let WasmStructrefValues = [];
let WasmArrayrefValues = [];
if (wasmGcEnabled()) {
    let { newStruct, newArray } = wasmEvalText(`
      (module
        (type $s (sub (struct)))
        (type $a (sub (array i32)))
        (func (export "newStruct") (result anyref)
            struct.new $s)
        (func (export "newArray") (result anyref)
            i32.const 0
            i32.const 0
            array.new $a)
      )`).exports;
    WasmStructrefValues.push(newStruct());
    WasmArrayrefValues.push(newArray());
}

let WasmGcObjectValues = WasmStructrefValues.concat(WasmArrayrefValues);

// Valid values for eqref
let WasmEqrefValues = [...WasmStructrefValues, ...WasmArrayrefValues];

// Valid values for i31ref
let MinI31refValue = -1 * Math.pow(2, 30);
let MaxI31refValue = Math.pow(2, 30) - 1;
let WasmI31refValues = [
  // first four 31-bit signed numbers
  MinI31refValue,
  MinI31refValue + 1,
  MinI31refValue + 2,
  MinI31refValue + 3,
  // five numbers around zero
  -2,
  -1,
  0,
  1,
  2,
  // last four 31-bit signed numbers
  MaxI31refValue - 3,
  MaxI31refValue - 2,
  MaxI31refValue - 1,
  MaxI31refValue,
];

// Valid and invalid values for anyref
let WasmAnyrefValues = [...WasmEqrefValues, ...WasmI31refValues];
let WasmNonAnyrefValues = [
    undefined,
    true,
    false,
    {x:1337},
    ["abracadabra"],
    13.37,
    -0,
    0x7fffffff + 0.1,
    -0x7fffffff - 0.1,
    0x80000000 + 0.1,
    -0x80000000 - 0.1,
    0xffffffff + 0.1,
    -0xffffffff - 0.1,
    Number.EPSILON,
    Number.MAX_SAFE_INTEGER,
    Number.MIN_SAFE_INTEGER,
    Number.MIN_VALUE,
    Number.MAX_VALUE,
    Number.NaN,
    "hi",
    37n,
    new Number(42),
    new Boolean(true),
    Symbol("status"),
    () => 1337,
    ...WasmFuncrefValues,
];

// Valid externref values
let WasmNonNullExternrefValues = [
    ...WasmNonAnyrefValues,
    ...WasmAnyrefValues
];
let WasmExternrefValues = [null, ...WasmNonNullExternrefValues];

// Common array utilities

// iota(n,k) creates an Array of length n with values k..k+n-1
function iota(len, k=0) {
    let xs = [];
    for ( let i=0 ; i < len ; i++ )
        xs.push(i+k);
    return xs;
}

// cross(A) where A is an array of length n creates an Array length n*n of
// two-element Arrays representing all pairs of elements of A.
function cross(xs) {
    let results = [];
    for ( let x of xs )
        for ( let y of xs )
            results.push([x,y]);
    return results;
}

// Remove all values equal to v from an array xs, comparing equal for NaN.
function remove(v, xs) {
    let result = [];
    for ( let w of xs ) {
        if (v === w || isNaN(v) && isNaN(w))
            continue;
        result.push(w);
    }
    return result;
}

// permute(A) where A is an Array returns an Array of Arrays, each inner Array a
// distinct permutation of the elements of A.  A is assumed not to have any
// elements that are pairwise equal in the sense of remove().
function permute(xs) {
    if (xs.length == 1)
        return [xs];
    let results = [];
    for (let v of xs)
        for (let tail of permute(remove(v, xs)))
            results.push([v, ...tail]);
    return results;
}

// interleave([a,b,c,...],[0,1,2,...]) => [a,0,b,1,c,2,...]
function interleave(xs, ys) {
    assertEq(xs.length, ys.length);
    let res = [];
    for ( let i=0 ; i < xs.length; i++ ) {
        res.push(xs[i]);
        res.push(ys[i]);
    }
    return res;
}

// assertSame([a,...],[b,...]) asserts that the two arrays have the same length
// and that they element-wise assertEq IGNORING Number/BigInt differences.  This
// predicate is in this file because it is wasm-specific.
function assertSame(got, expected) {
    assertEq(got.length, expected.length);
    for ( let i=0; i < got.length; i++ ) {
        let g = got[i];
        let e = expected[i];
        if (typeof g != typeof e) {
            if (typeof g == "bigint")
                e = BigInt(e);
            else if (typeof e == "bigint")
                g = BigInt(g);
        }
        assertEq(g, e);
    }
}

// TailCallIterations is selected to be large enough to trigger
// "too much recursion", but not to be slow.
var TailCallIterations = getBuildConfiguration("simulator") ? 1000 : 100000;
// TailCallBallast is selected to spill registers as parameters.
var TailCallBallast = 30;
