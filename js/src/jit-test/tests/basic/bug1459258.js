if (!('oomTest' in this))
    quit();
oomTest(function() {
    return [0, Math.PI, NaN, Infinity, true, false, Symbol(), Math.tan,
            Reflect, Proxy, print, assertEq, Array, String, Boolean, Number, parseInt,
            parseFloat, Math.sin, Math.cos, Math.abs, Math.pow, Math.sqrt,
            Uint8Array, Int8Array, Int32Array, Int16Array, Uint16Array];
});
