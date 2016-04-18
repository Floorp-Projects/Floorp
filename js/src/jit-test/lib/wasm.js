if (!wasmIsSupported())
    quit();

load(libdir + "asserts.js");

function wasmEvalText(str, imports) {
    return Wasm.instantiateModule(wasmTextToBinary(str), imports).exports;
}

function mismatchError(actual, expect) {
    var str = `type mismatch: expression has type ${actual} but expected ${expect}`;
    return RegExp(str);
}

function hasI64() {
    return getBuildConfiguration().x64;
}

// Assert that the expected value is equal to the int64 value, as passed by
// Baldr with --wasm-extra-tests {low: int32, high: int32}.
// - if the expected value is in the int32 range, it can be just a number.
// - otherwise, an object with the properties "high" and "low".
function assertEqI64(obj, expect) {
    assertEq(typeof obj, 'object');
    assertEq(typeof expect === 'object' || typeof expect === 'number', true);

    let {low, high} = obj;
    if (typeof expect === 'number') {
        assertEq(expect, expect | 0, "in int32 range");
        assertEq(low, expect | 0, "low");
        assertEq(high, expect < 0 ? -1 : 0), "high";
    } else {
        assertEq(typeof expect.low, 'number');
        assertEq(typeof expect.high, 'number');
        assertEq(low, expect.low | 0, "low");
        assertEq(high, expect.high | 0, "high");
    }
}

function createI64(val) {
    let ret;
    if (typeof val === 'number') {
        assertEq(val, val|0, "number input to createI64 must be an int32");
        ret = { low: val, high: val < 0 ? -1 : 0 };
    } else {
        assertEq(typeof val, 'string');
        assertEq(val.slice(0, 2), "0x");
        val = val.slice(2).padStart(16, '0');
        ret = { low: parseInt(val.slice(8, 16), 16), high: parseInt(val.slice(0, 8), 16) };
    }
    return ret;
}
