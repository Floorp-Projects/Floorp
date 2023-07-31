// |jit-test| 

load(libdir + "asserts.js");

function f5() { } 

function f0() {
    const v15 = new Uint8Array();
    f5 &&= v15;
    const v17 = new Uint8Array();
    const v16 = wrapWithProto(v17, f5);
    v16.with();
}
assertThrowsInstanceOf(() => f0(), RangeError);
