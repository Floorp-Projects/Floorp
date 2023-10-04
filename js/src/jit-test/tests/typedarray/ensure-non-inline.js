const constructors = [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array ];

function messWith(view, obj) {
    view[0] = 1;
    view[1] = 1;
    view[2] = 2;
    view[3] = 3;
    ensureNonInline(obj);
    assertEq(view[0], 1);
    assertEq(view[1], 1);
    assertEq(view[2], 2);
    assertEq(view[3], 3);
}

function test() {
    // Bufferless
    for (const ctor of constructors) {
        let small = new ctor(4);
        messWith(small, small);
        let big = new ctor(4000);
        messWith(big, big);
    }

    // With buffer, single view, operate on view
    for (const ctor of constructors) {
        let ab = new ArrayBuffer(96);
        const small = new ctor(ab);
        messWith(small, small);
        ab = new ArrayBuffer(4000);
        const big = new ctor(ab);
        messWith(big, big);
    }

    // With buffer, single view, operate on ArrayBuffer
    for (const ctor of constructors) {
        let ab = new ArrayBuffer(96);
        const small = new ctor(ab);
        messWith(small, ab);
        ab = new ArrayBuffer(4000);
        const big = new ctor(ab);
        messWith(big, ab);
    }

    // With buffer, dual view, operate on view
    for (const ctor of constructors) {
        let ab = new ArrayBuffer(96);
        let view0 = new Uint8Array(ab);
        const small = new ctor(ab);
        messWith(small, small);
        ab = new ArrayBuffer(4000);
        view0 = new Uint8Array(ab);
        const big = new ctor(ab);
        messWith(big, big);
    }

    // With buffer, dual view, operate on ArrayBuffer
    for (const ctor of constructors) {
        let ab = new ArrayBuffer(96);
        let view0 = new Uint8Array(ab);
        const small = new ctor(ab);
        messWith(small, ab);
        ab = new ArrayBuffer(4000);
        view0 = new Uint8Array(ab);
        const big = new ctor(ab);
        messWith(big, ab);
    }
}

try {
    ensureNonInline(new Array(3));
    assertEq(false, true);
} catch (e) {
    assertEq(e.message.includes("unhandled type"), true);
}

test();
