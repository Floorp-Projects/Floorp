
function typeOf(o) {
    assertEq(typeof o, "object");
}

typeOf([]);
typeOf(new Float32Array());
typeOf(new Int32Array());
typeOf(new ArrayBuffer());
