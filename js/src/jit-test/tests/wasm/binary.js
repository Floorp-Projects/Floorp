load(libdir + "wasm.js");

if (!wasmIsSupported())
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
const importSectionStr = "import";
const exportSectionStr = "export";
const codeSectionStr = "code";
const funcSubsectionStr = "func";

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
    for (let b of array)
        assertEq(b < 256, true);
    return Uint8Array.from(array).buffer;
}

function varU32(u32) {
    // TODO
    assertEq(u32 < 128, true);
    return [u32];
}

const U32MAX_LEB = [255, 255, 255, 255, 15];

function moduleHeaderThen(...rest) {
    return [magic0, magic1, magic2, magic3, ver0, ver1, ver2, ver3, ...rest];
}

assertErrorMessage(() => wasmEval(toBuf([])), TypeError, magicError);
assertErrorMessage(() => wasmEval(toBuf([42])), TypeError, magicError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2])), TypeError, magicError);
assertErrorMessage(() => wasmEval(toBuf([1,2,3,4])), TypeError, magicError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2, magic3])), TypeError, versionError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2, magic3, 1])), TypeError, versionError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2, magic3, ver0])), TypeError, versionError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2, magic3, ver0, ver1, ver2])), TypeError, versionError);

var o = wasmEval(toBuf(moduleHeaderThen(0)));
assertEq(Object.getOwnPropertyNames(o).length, 0);

assertErrorMessage(() => wasmEval(toBuf(moduleHeaderThen(1))), TypeError, sectionError);
assertErrorMessage(() => wasmEval(toBuf(moduleHeaderThen(0, 1))), TypeError, extraError);

function cstring(name) {
    return (name + '\0').split('').map(c => c.charCodeAt(0));
}

function sectionLength(length) {
    var i32 = new Uint32Array(1);
    i32[0] = length;
    return new Uint8Array(i32.buffer);
}

function moduleWithSections(sectionArray) {
    var bytes = moduleHeaderThen();
    for (let section of sectionArray) {
        bytes.push(...cstring(section.name));
        bytes.push(...sectionLength(section.body.length));
        bytes.push(...section.body);
    }
    bytes.push(0);
    return bytes;
}

function sigSection(sigs) {
    var body = [];
    body.push(...varU32(sigs.length));
    for (let sig of sigs) {
        body.push(...varU32(sig.args.length));
        body.push(...varU32(sig.ret));
        for (let arg of sig.args)
            body.push(...varU32(arg));
    }
    return { name: sigSectionStr, body };
}

function declSection(decls) {
    var body = [];
    body.push(...varU32(decls.length));
    for (let decl of decls)
        body.push(...varU32(decl));
    return { name: declSectionStr, body };
}

function codeSection(funcs) {
    var body = [];
    body.push(...varU32(funcs.length));
    for (let func of funcs) {
        body.push(...cstring(funcSubsectionStr));
        var locals = varU32(func.locals.length);
        for (let local of func.locals)
            locals.push(...varU32(local));
        body.push(...sectionLength(locals.length + func.body.length));
        body = body.concat(locals, func.body);
    }
    return { name: codeSectionStr, body };
}

function importSection(imports) {
    var body = [];
    body.push(...varU32(imports.length));
    for (let imp of imports) {
        body.push(...cstring(funcSubsectionStr));
        body.push(...varU32(imp.sigIndex));
        body.push(...cstring(imp.module));
        body.push(...cstring(imp.func));
    }
    return { name: importSectionStr, body };
}

const trivialSigSection = sigSection([{args:[], ret:VoidCode}]);
const trivialDeclSection = declSection([0]);
const trivialCodeSection = codeSection([{locals:[], body:[0, 0]}]);

assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([ {name: sigSectionStr, body: U32MAX_LEB, } ]))), Error, /too many signatures/);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([ {name: sigSectionStr, body: [1, ...U32MAX_LEB], } ]))), Error, /too many arguments in signature/);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([trivialSigSection, {name: declSectionStr, body: U32MAX_LEB, }]))), Error, /too many functions/);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([trivialSigSection, {name: importSectionStr, body: U32MAX_LEB, }]))), Error, /too many imports/);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([trivialSigSection, {name: exportSectionStr, body: U32MAX_LEB, }]))), Error, /too many exports/);

assertThrowsInstanceOf(() => wasmEval(toBuf(moduleWithSections([{name: sigSectionStr, body: [1]}]))), TypeError);
assertThrowsInstanceOf(() => wasmEval(toBuf(moduleWithSections([{name: sigSectionStr, body: [1, 1, 0]}]))), TypeError);

wasmEval(toBuf(moduleWithSections([sigSection([])])));
wasmEval(toBuf(moduleWithSections([sigSection([{args:[], ret:VoidCode}])])));
wasmEval(toBuf(moduleWithSections([sigSection([{args:[I32Code], ret:VoidCode}])])));

assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([sigSection([{args:[], ret:100}])]))), TypeError, /bad expression type/);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([sigSection([{args:[100], ret:VoidCode}])]))), TypeError, /bad value type/);

assertThrowsInstanceOf(() => wasmEval(toBuf(moduleWithSections([sigSection([]), declSection([0])]))), TypeError, /signature index out of range/);
assertThrowsInstanceOf(() => wasmEval(toBuf(moduleWithSections([trivialSigSection, declSection([1])]))), TypeError, /signature index out of range/);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([trivialSigSection, declSection([0])]))), TypeError, /fewer function definitions than declarations/);
wasmEval(toBuf(moduleWithSections([trivialSigSection, trivialDeclSection, trivialCodeSection])));

assertThrowsInstanceOf(() => wasmEval(toBuf(moduleWithSections([trivialSigSection, {name: importSectionStr, body:[]}]))), TypeError);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([importSection([{sigIndex:0, module:"a", func:"b"}])]))), TypeError, /signature index out of range/);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([trivialSigSection, importSection([{sigIndex:1, module:"a", func:"b"}])]))), TypeError, /signature index out of range/);
wasmEval(toBuf(moduleWithSections([trivialSigSection, importSection([])])));
wasmEval(toBuf(moduleWithSections([trivialSigSection, importSection([{sigIndex:0, module:"a", func:""}])])), {a:()=>{}});
