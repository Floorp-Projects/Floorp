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

// Asserts in Baldr test mode that NaN payloads match.
function assertEqNaN(x, y) {
    if (typeof x === 'number') {
        assertEq(Number.isNaN(x), Number.isNaN(y));
        return;
    }

    assertEq(typeof x === 'object' &&
             typeof x.nan_low === 'number',
             true,
             "assertEqNaN args must have shape {nan_high, nan_low}");

    assertEq(typeof y === 'object' &&
             typeof y.nan_low === 'number',
             true,
             "assertEqNaN args must have shape {nan_high, nan_low}");

    assertEq(typeof x.nan_high,
             typeof y.nan_high,
             "both args must have nan_high, or none");

    assertEq(x.nan_high, y.nan_high, "assertEqNaN nan_high don't match");
    if (typeof x.nan_low !== 'undefined')
        assertEq(x.nan_low, y.nan_low, "assertEqNaN nan_low don't match");
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
