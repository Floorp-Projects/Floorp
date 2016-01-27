load(libdir + "wasm.js");

if (!this.wasmEval)
    quit();

// MagicNumber = 0x4d534100
const magic0 = 0;
const magic1 = 97;  // 'a'
const magic2 = 115; // 's'
const magic3 = 109; // 'm'

// EncodingVersion = -1 (to be changed to 1 at some point in the future)
const ver0 = 0xff;
const ver1 = 0xff;
const ver2 = 0xff;
const ver3 = 0xff;

// Section names
const sigSectionStr = "sig";
const declSectionStr = "decl";
const exportSectionStr = "export";
const codeSectionStr = "code";

const magicError = /failed to match magic number/;
const versionError = /failed to match binary version/;
const extraError = /failed to consume all bytes of module/;
const sectionError = /failed to read section name/;

const I32Code = 0;
const I64Code = 1;
const F32Code = 2;
const F64Code = 3;
const I32x4Code = 4;
const F32x4Code = 5;
const B32x4Code = 6;
const VoidCode = 7;

function toBuf(array) {
    for (var b of array)
        assertEq(b < 256, true);
    return Uint8Array.from(array).buffer;
}

function varU32(u32) {
    // TODO
    assertEq(u32 < 128, true);
    return [u32];
}

function moduleHeaderThen(...rest) {
    return [magic0, magic1, magic2, magic3, ver0, ver1, ver2, ver3, ...rest];
}

assertErrorMessage(() => wasmEval(toBuf([])), Error, magicError);
assertErrorMessage(() => wasmEval(toBuf([42])), Error, magicError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2])), Error, magicError);
assertErrorMessage(() => wasmEval(toBuf([1,2,3,4])), Error, magicError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2, magic3])), Error, versionError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2, magic3, 1])), Error, versionError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2, magic3, ver0])), Error, versionError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2, magic3, ver0, ver1, ver2])), Error, versionError);

var o = wasmEval(toBuf(moduleHeaderThen(0)));
assertEq(Object.getOwnPropertyNames(o).length, 0);

assertErrorMessage(() => wasmEval(toBuf(moduleHeaderThen(1))), Error, sectionError);
assertErrorMessage(() => wasmEval(toBuf(moduleHeaderThen(0, 1))), Error, extraError);

function sectionName(name) {
    return (name + '\0').split('').map(c => c.charCodeAt(0));
}

function sectionLength(length) {
    var i32 = new Uint32Array(1);
    i32[0] = length;
    return new Uint8Array(i32.buffer);
}

function moduleWithSections(sectionArray) {
    var bytes = moduleHeaderThen();
    for (section of sectionArray) {
        bytes.push(...sectionName(section.name));
        bytes.push(...sectionLength(section.body.length));
        bytes.push(...section.body);
    }
    bytes.push(0);
    return bytes;
}

function sigSection(sigs) {
    var body = [];
    body.push(...varU32(sigs.length));
    for (var sig of sigs) {
        body.push(...varU32(sig.args.length));
        body.push(...varU32(sig.ret));
        for (var arg of sig.args)
            body.push(...varU32(arg));
    }
    return { name: sigSectionStr, body };
}

assertThrowsInstanceOf(() => wasmEval(toBuf(moduleWithSections([{name: sigSectionStr, body: [1]}]))), Error);
assertThrowsInstanceOf(() => wasmEval(toBuf(moduleWithSections([{name: sigSectionStr, body: [1, 1, 0]}]))), Error);

wasmEval(toBuf(moduleWithSections([sigSection([])])));
wasmEval(toBuf(moduleWithSections([sigSection([{args:[], ret:VoidCode}])])));
wasmEval(toBuf(moduleWithSections([sigSection([{args:[I32Code], ret:VoidCode}])])));

assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([sigSection([{args:[], ret:100}])]))), Error, /bad expression type/);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([sigSection([{args:[100], ret:VoidCode}])]))), Error, /bad value type/);

function declSection(decls) {
    var body = [];
    body.push(...varU32(decls.length));
    for (var decl of decls)
        body.push(...varU32(decl));
    return { name: declSectionStr, body };
}

assertThrowsInstanceOf(() => wasmEval(toBuf(moduleWithSections([sigSection([]), declSection([0])]))), Error, /signature index out of range/);
assertThrowsInstanceOf(() => wasmEval(toBuf(moduleWithSections([sigSection([{args:[], ret:VoidCode}]), declSection([1])]))), Error, /signature index out of range/);

assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([sigSection([{args:[], ret:VoidCode}]), declSection([0])]))), Error, /fewer function definitions than declarations/);
