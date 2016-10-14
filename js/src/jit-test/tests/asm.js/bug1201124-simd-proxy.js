// |jit-test| test-also-noasmjs
load(libdir + "asm.js");
load(libdir + "asserts.js");

if (typeof newGlobal !== 'function')
    quit();

var stdlib = new (newGlobal().Proxy)(this, new Proxy({
    simdGet: 0,
    getOwnPropertyDescriptor(t, pk) {
        if (pk === "SIMD" && this.simdGet++ === 1) {
            return {};
        }
        return Reflect.getOwnPropertyDescriptor(t, pk);
    }
}, {
    get(t, pk, r) {
        print("trap", pk);
        return Reflect.get(t, pk, r);
    }
}));

var m = asmCompile('stdlib', '"use asm"; var i4=stdlib.SIMD.Int32x4; var i4add=i4.add; return {}');

assertAsmLinkFail(m, stdlib);
