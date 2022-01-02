let a = wrapWithProto(new Int16Array([300]), {});
let b = new Uint8ClampedArray(a);
assertEq(b[0], 255);
