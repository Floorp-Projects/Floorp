if (!wasmIsSupported())
    quit();

load(libdir + "asserts.js");

function wasmEvalText(str, imports) {
    let binary = wasmTextToBinary(str);
    let valid = WebAssembly.validate(binary);

    let m;
    try {
        m = new WebAssembly.Module(binary);
        assertEq(valid, true);
    } catch(e) {
        if (!e.toString().match(/out of memory/))
            assertEq(valid, false);
        throw e;
    }

    return new WebAssembly.Instance(m, imports);
}

function wasmValidateText(str) {
    assertEq(WebAssembly.validate(wasmTextToBinary(str)), true);
}

function wasmFailValidateText(str, pattern) {
    let binary = wasmTextToBinary(str);
    assertEq(WebAssembly.validate(binary), false);
    assertErrorMessage(() => new WebAssembly.Module(binary), WebAssembly.CompileError, pattern);
}

function mismatchError(actual, expect) {
    var str = `type mismatch: expression has type ${actual} but expected ${expect}`;
    return RegExp(str);
}

const emptyStackError = /from empty stack/;
const unusedValuesError = /unused values not explicitly dropped by end of block/;

function jsify(wasmVal) {
    if (wasmVal === 'nan')
        return NaN;
    if (wasmVal === 'infinity')
        return Infinity;
    if (wasmVal === '-infinity')
        return Infinity;
    if (wasmVal === '-0')
        return -0;
    return wasmVal;
}

function makeF32Assert(i, func, expected, params=[]) {
    return `
    (func (export "assert_${i}") (result i32)
     ${ params.join('\n') }
     call ${func}
     i32.reinterpret/f32
     i32.const ${expected}
     i32.eq
    )`;
}

function makeF64Assert(i, func, expected, params=[]) {
    return `
    (func (export "assert_${i}") (result i32)
     ${ params.join('\n') }
     call ${func}
     i64.reinterpret/f64
     i64.const ${expected}
     i64.eq
    )`;
}

function wasmAssert(src, assertions) {
    let newSrc = src.substr(0, src.lastIndexOf(')'));
    let i = 0;
    for (let a of assertions) {
        if (a.type === 'f32')
            newSrc += makeF32Assert(i++, a.func, a.expected, a.params);
        if (a.type === 'f64')
            newSrc += makeF64Assert(i++, a.func, a.expected, a.params);
    }
    newSrc += ')';

    let { exports } = wasmEvalText(newSrc);

    for (let j = 0; j < i; j++) {
        let { func, expected, params } = assertions[j];
        let paramText = params ? params.join(', ') : '';
        assertEq(exports['assert_' + j](), 1,
                 `Unexpected value when running ${func}(${paramText}), expecting ${expected}.`);
    }
}

// Assert that the expected value is equal to the int64 value, as passed by
// Baldr: {low: int32, high: int32}.
// - if the expected value is in the int32 range, it can be just a number.
// - otherwise, an object with the properties "high" and "low".
function assertEqI64(observed, expect) {
    assertEq(typeof observed, 'object', "observed must be an i64 object");
    assertEq(typeof expect === 'object' || typeof expect === 'number', true,
             "expect must be an i64 object or number");

    let {low, high} = observed;
    if (typeof expect === 'number') {
        assertEq(expect, expect | 0, "in int32 range");
        assertEq(low, expect | 0, "low 32 bits don't match");
        assertEq(high, expect < 0 ? -1 : 0, "high 32 bits don't match"); // sign extension
    } else {
        assertEq(typeof expect.low, 'number');
        assertEq(typeof expect.high, 'number');
        assertEq(low, expect.low | 0, "low 32 bits don't match");
        assertEq(high, expect.high | 0, "high 32 bits don't match");
    }
}

function createI64(val) {
    let ret;
    if (typeof val === 'number') {
        assertEq(val, val|0, "number input to createI64 must be an int32");
        ret = {
            low: val,
            high: val < 0 ? -1 : 0 // sign extension
        };
    } else {
        assertEq(typeof val, 'string');
        assertEq(val.slice(0, 2), "0x");
        val = val.slice(2).padStart(16, '0');
        ret = {
            low: parseInt(val.slice(8, 16), 16),
            high: parseInt(val.slice(0, 8), 16)
        };
    }
    return ret;
}

function _wasmFullPassInternal(assertValueFunc, text, expected, maybeImports, ...args) {
    let binary = wasmTextToBinary(text);
    assertEq(WebAssembly.validate(binary), true, "Must validate.");

    let module = new WebAssembly.Module(binary);
    let instance = new WebAssembly.Instance(module, maybeImports);
    assertEq(typeof instance.exports.run, 'function', "A 'run' function must be exported.");
    assertValueFunc(instance.exports.run(...args), expected, "Initial module must return the expected result.");

    let retext = wasmBinaryToText(binary);
    let rebinary = wasmTextToBinary(retext);

    assertEq(WebAssembly.validate(rebinary), true, "Recreated binary must validate.");
    let remodule = new WebAssembly.Module(rebinary);
    let reinstance = new WebAssembly.Instance(remodule, maybeImports);
    assertValueFunc(reinstance.exports.run(...args), expected, "Reformed module must return the expected result");
}

// Fully test a module:
// - ensure it validates.
// - ensure it compiles and produces the expected result.
// - ensure textToBinary(binaryToText(binary)) = binary
// Preconditions:
// - the binary module must export a function called "run".
function wasmFullPass(text, expected, maybeImports, ...args) {
    _wasmFullPassInternal(assertEq, text, expected, maybeImports, ...args);
}

function wasmFullPassI64(text, expected, maybeImports, ...args) {
    _wasmFullPassInternal(assertEqI64, text, expected, maybeImports, ...args);
}

function wasmRunWithDebugger(wast, lib, init, done) {
    let g = newGlobal('');
    let dbg = new Debugger(g);

    g.eval(`
var wasm = wasmTextToBinary('${wast}');
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

function wasmGetScriptBreakpoints(wasmScript) {
    var result = [];
    var sourceText = wasmScript.source.text;
    sourceText.split('\n').forEach(function (line, i) {
        var lineOffsets = wasmScript.getLineOffsets(i + 1);
        if (lineOffsets.length === 0)
            return;
        assertEq(lineOffsets.length, 1);
        result.push({str: line.trim(), line: i + 1, offset: lineOffsets[0]});
    });
    return result;
}
