load(libdir + "wasm.js");

if (!this.wasmEval)
    quit();

// MagicNumber = 0x4d534100
var magic0 = 0;
var magic1 = 97;  // 'a'
var magic2 = 115; // 's'
var magic3 = 109; // 'm'

// EncodingVersion = -1
var ver0 = 0xff;
var ver1 = 0xff;
var ver2 = 0xff;
var ver3 = 0xff;

var magicError = /failed to match magic number/;
var versionError = /failed to match binary version/;
var extraError = /failed to consume all bytes of module/;

assertErrorMessage(() => wasmEval(Uint8Array.of().buffer), Error, magicError);
assertErrorMessage(() => wasmEval(Uint8Array.of(42).buffer), Error, magicError);
assertErrorMessage(() => wasmEval(Uint8Array.of(magic0, magic1, magic2).buffer), Error, magicError);
assertErrorMessage(() => wasmEval(Uint8Array.of(1,2,3,4).buffer), Error, magicError);
assertErrorMessage(() => wasmEval(Uint8Array.of(magic0, magic1, magic2, magic3).buffer), Error, versionError);
assertErrorMessage(() => wasmEval(Uint8Array.of(magic0, magic1, magic2, magic3, 1).buffer), Error, versionError);
assertErrorMessage(() => wasmEval(Uint8Array.of(magic0, magic1, magic2, magic3, ver0).buffer), Error, versionError);
assertErrorMessage(() => wasmEval(Uint8Array.of(magic0, magic1, magic2, magic3, ver0, ver1, ver2).buffer), Error, versionError);
assertErrorMessage(() => wasmEval(Uint8Array.of(magic0, magic1, magic2, magic3, ver0, ver1, ver2, ver3, 0, 1).buffer), Error, extraError);

var o = wasmEval(Uint8Array.of(magic0, magic1, magic2, magic3, ver0, ver1, ver2, ver3, 0).buffer);
assertEq(Object.getOwnPropertyNames(o).length, 0);
