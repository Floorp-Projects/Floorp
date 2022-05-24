// Testing runtime execution of select + comparison operations.
// Normally they are folded into shorter/faster sequence than select alone.

const floatOps = {
    lt(a, b) { return a < b ? 0 : 1; },
    le(a, b) { return a <= b ? 0 : 1; },
    gt(a, b) { return a > b ? 0 : 1; },
    ge(a, b) { return a >= b ? 0 : 1; },
    eq(a, b) { return a === b ? 0 : 1; },
    ne(a, b) { return a !== b ? 0 : 1; },
}

for (let ty of ['f32', 'f64']) {
    for (let op of ['lt', 'le', 'gt', 'ge', 'eq', 'ne']) {
        const module = new WebAssembly.Module(wasmTextToBinary(`(module
            (memory (export "memory") 1 1)
            (func (export "test") (result i32)
                i32.const 128
                i32.load8_u
                i32.const 129
                i32.load8_u
                i32.const 0
                ${ty}.load
                i32.const ${ty == 'f32' ? 4 : 8}
                ${ty}.load
                ${ty}.${op}
                select
            )
            (data (i32.const 128) "\\00\\01"))`));
        const instance = new WebAssembly.Instance(module);
        const arr = new (ty == 'f32' ? Float32Array : Float64Array)(instance.exports.memory.buffer);
        for (let [a, b] of cross(
            [0, 1, -1e100, Infinity, -Infinity, 1e100, -1e-10, 1/-Infinity, NaN]
        )) {
            arr[0] = a; arr[1] = b;
            assertEq(instance.exports.test(), floatOps[op](arr[0], arr[1]))
        }
    }
}

const intOps = {
    lt(a, b) { return a < b ? 0 : 1; },
    le(a, b) { return a <= b ? 0 : 1; },
    gt(a, b) { return a > b ? 0 : 1; },
    ge(a, b) { return a >= b ? 0 : 1; },
    eq(a, b) { return a === b ? 0 : 1; },
    ne(a, b) { return a !== b ? 0 : 1; },
}

for (let [ty, signed] of [['i32', true], ['i32', false], ['i64', true], ['i64', false]]) {
    for (let op of ['lt', 'le', 'gt', 'ge', 'eq', 'ne']) {
        const module = new WebAssembly.Module(wasmTextToBinary(`(module
            (memory (export "memory") 1 1)
            (func (export "test") (result i32)
                i32.const 128
                i32.load8_u
                i32.const 129
                i32.load8_u
                i32.const 0
                ${ty}.load
                i32.const ${ty == 'i32' ? 4 : 8}
                ${ty}.load
                ${ty}.${op}${op[0] == 'l' || op[0] == 'g' ? (signed ? '_s' : '_u') : ''}
                select
            )
            (data (i32.const 128) "\\00\\01"))`));
        const instance = new WebAssembly.Instance(module);
        const arr = new (ty == 'i32' ? (signed ? Int32Array : Uint32Array) :
                                       (signed ? BigInt64Array : BigUint64Array))
                        (instance.exports.memory.buffer);
        const c = ty == 'i32' ? (a => a|0) : BigInt;
        for (let [a, b] of cross(
            [c(0), ~c(0), c(1), ~c(1), c(1) << c(8), ~c(1) << c(12)]
        )) {
            arr[0] = a; arr[1] = b;
            assertEq(instance.exports.test(), intOps[op](arr[0], arr[1]))
        }
    }
}
