if (!wasmIsSupported())
    quit();

load(libdir + "asserts.js");

function hasWasmAtomics() {
    try {
	// Typically the parser will be in sync with the rest of wasm and
	// parsing will throw if shared memory is not supported.
        let bin = wasmTextToBinary(`(module (memory 1 1 shared)
				     (func (result i32)
				      (i32.atomic.load (i32.const 0)))
				     (export "" 0))`);

	// If parsing doesn't throw, let's check that the validator agrees.
        return WebAssembly.validate(bin);
    } catch (e) {
        return false;
    }
}

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
         i32.reinterpret/f32
         i32.const ${expected}
         i32.eq`;
                    break;
                case 'f64':
                    newSrc += `
         i64.reinterpret/f64
         i64.const ${expected}
         i64.eq`;
                    break;
                case 'i64':
                    newSrc += `
         i64.const ${expected}
         i64.eq`;
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

function wasmAssert(src, assertions, maybeImports = {}) {
    let { exports } = wasmEvalText(_augmentSrc(src, assertions), maybeImports);
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

    let retext = wasmBinaryToText(binary);
    let rebinary = wasmTextToBinary(retext);

    assertEq(WebAssembly.validate(rebinary), true, "Recreated binary must validate.");
    let remodule = new WebAssembly.Module(rebinary);
    let reinstance = new WebAssembly.Instance(remodule, maybeImports);
    assertEq(reinstance.exports.run(...args), expected, "Reformed module must return the expected result");
}

// Ditto, but expects a function named '$run' instead of exported with this name.
function wasmFullPassI64(text, expected, maybeImports, ...args) {
    let binary = wasmTextToBinary(text);
    assertEq(WebAssembly.validate(binary), true, "Must validate.");

    let augmentedSrc = _augmentSrc(text, [ { type: 'i64', func: '$run', args, expected } ]);
    let augmentedBinary = wasmTextToBinary(augmentedSrc);
    new WebAssembly.Instance(new WebAssembly.Module(augmentedBinary), maybeImports).exports.assert_0();

    let retext = wasmBinaryToText(augmentedBinary);
    new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(retext)), maybeImports).exports.assert_0();
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
