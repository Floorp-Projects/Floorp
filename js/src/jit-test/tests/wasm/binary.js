load(libdir + "wasm.js");

// MagicNumber = 0x6d736100;
const magic0 = 0x00;  // '\0'
const magic1 = 0x61;  // 'a'
const magic2 = 0x73;  // 's'
const magic3 = 0x6d;  // 'm'

// EncodingVersion = 10 (to be changed to 1 at some point in the future)
const ver0 = 0x0a;
const ver1 = 0x00;
const ver2 = 0x00;
const ver3 = 0x00;

// Section names
const sigId                = "signatures";
const importId             = "import_table";
const functionSignaturesId = "function_signatures";
const functionTableId      = "function_table";
const exportTableId        = "export_table";
const functionBodiesId     = "function_bodies";
const dataSegmentsId       = "data_segments";

const magicError = /failed to match magic number/;
const versionError = /failed to match binary version/;
const unknownSectionError = /failed to skip unknown section at end/;
const sectionError = /failed to start section/;

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

const wasmEval = Wasm.instantiateModule;

assertErrorMessage(() => wasmEval(toBuf([])), TypeError, magicError);
assertErrorMessage(() => wasmEval(toBuf([42])), TypeError, magicError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2])), TypeError, magicError);
assertErrorMessage(() => wasmEval(toBuf([1,2,3,4])), TypeError, magicError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2, magic3])), TypeError, versionError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2, magic3, 1])), TypeError, versionError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2, magic3, ver0])), TypeError, versionError);
assertErrorMessage(() => wasmEval(toBuf([magic0, magic1, magic2, magic3, ver0, ver1, ver2])), TypeError, versionError);

var o = wasmEval(toBuf(moduleHeaderThen()));
assertEq(Object.getOwnPropertyNames(o).length, 0);

wasmEval(toBuf(moduleHeaderThen(1, 0)));        // unknown section containing 0-length string
wasmEval(toBuf(moduleHeaderThen(2, 1, 0)));     // unknown section containing 1-length string ("\0")
wasmEval(toBuf(moduleHeaderThen(1, 0,  1, 0)));
wasmEval(toBuf(moduleHeaderThen(1, 0,  2, 1, 0)));
wasmEval(toBuf(moduleHeaderThen(1, 0,  2, 1, 0)));

assertErrorMessage(() => wasmEval(toBuf(moduleHeaderThen(1))), TypeError, sectionError);
assertErrorMessage(() => wasmEval(toBuf(moduleHeaderThen(0, 1))), TypeError, sectionError);
assertErrorMessage(() => wasmEval(toBuf(moduleHeaderThen(0, 0))), TypeError, unknownSectionError);

function cstring(name) {
    return (name + '\0').split('').map(c => c.charCodeAt(0));
}

function string(name) {
    return name.split('').map(c => c.charCodeAt(0));
}

function moduleWithSections(sectionArray) {
    var bytes = moduleHeaderThen();
    for (let section of sectionArray) {
        var nameLength = varU32(section.name.length);
        bytes.push(...varU32(nameLength.length + section.name.length + section.body.length));
        bytes.push(...nameLength);
        bytes.push(...string(section.name));
        bytes.push(...section.body);
    }
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
    return { name: sigId, body };
}

function declSection(decls) {
    var body = [];
    body.push(...varU32(decls.length));
    for (let decl of decls)
        body.push(...varU32(decl));
    return { name: functionSignaturesId, body };
}

function funcBody(func) {
    var body = varU32(func.locals.length);
    for (let local of func.locals)
        body.push(...varU32(local));
    body = body.concat(...func.body);
    body.splice(0, 0, ...varU32(body.length));
    return body;
}

function bodySection(bodies) {
    var body = [].concat(...bodies);
    return { name: functionBodiesId, body };
}

function importSection(imports) {
    var body = [];
    body.push(...varU32(imports.length));
    for (let imp of imports) {
        body.push(...varU32(imp.sigIndex));
        body.push(...cstring(imp.module));
        body.push(...cstring(imp.func));
    }
    return { name: importId, body };
}

function tableSection(elems) {
    var body = [];
    body.push(...varU32(elems.length));
    for (let i of elems)
        body.push(...varU32(i));
    return { name: functionTableId, body };
}

const v2vSig = {args:[], ret:VoidCode};
const i2vSig = {args:[I32Code], ret:VoidCode};
const v2vBody = funcBody({locals:[], body:[]});

assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([ {name: sigId, body: U32MAX_LEB, } ]))), TypeError, /too many signatures/);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([ {name: sigId, body: [1, ...U32MAX_LEB], } ]))), TypeError, /too many arguments in signature/);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([sigSection([{args:[], ret:VoidCode}, {args:[], ret:VoidCode}])]))), TypeError, /duplicate signature/);

assertThrowsInstanceOf(() => wasmEval(toBuf(moduleWithSections([{name: sigId, body: [1]}]))), TypeError);
assertThrowsInstanceOf(() => wasmEval(toBuf(moduleWithSections([{name: sigId, body: [1, 1, 0]}]))), TypeError);

wasmEval(toBuf(moduleWithSections([sigSection([])])));
wasmEval(toBuf(moduleWithSections([sigSection([v2vSig])])));
wasmEval(toBuf(moduleWithSections([sigSection([i2vSig])])));
wasmEval(toBuf(moduleWithSections([sigSection([v2vSig, i2vSig])])));

assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([sigSection([{args:[], ret:100}])]))), TypeError, /bad expression type/);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([sigSection([{args:[100], ret:VoidCode}])]))), TypeError, /bad value type/);

assertThrowsInstanceOf(() => wasmEval(toBuf(moduleWithSections([sigSection([]), declSection([0])]))), TypeError, /signature index out of range/);
assertThrowsInstanceOf(() => wasmEval(toBuf(moduleWithSections([sigSection([v2vSig]), declSection([1])]))), TypeError, /signature index out of range/);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([sigSection([v2vSig]), declSection([0])]))), TypeError, /expected function bodies/);
wasmEval(toBuf(moduleWithSections([sigSection([v2vSig]), declSection([0]), bodySection([v2vBody])])));

assertThrowsInstanceOf(() => wasmEval(toBuf(moduleWithSections([sigSection([v2vSig]), {name: importId, body:[]}]))), TypeError);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([importSection([{sigIndex:0, module:"a", func:"b"}])]))), TypeError, /signature index out of range/);
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([sigSection([v2vSig]), importSection([{sigIndex:1, module:"a", func:"b"}])]))), TypeError, /signature index out of range/);
wasmEval(toBuf(moduleWithSections([sigSection([v2vSig]), importSection([])])));
wasmEval(toBuf(moduleWithSections([sigSection([v2vSig]), importSection([{sigIndex:0, module:"a", func:""}])])), {a:()=>{}});

wasmEval(toBuf(moduleWithSections([
    sigSection([v2vSig]),
    importSection([{sigIndex:0, module:"a", func:""}]),
    declSection([0]),
    bodySection([v2vBody])])), {a:()=>{}});

assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([ {name: dataSegmentsId, body: [], } ]))), TypeError, /data section requires a memory section/);

wasmEval(toBuf(moduleWithSections([tableSection([])])));
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([tableSection([0])]))), TypeError, /table element out of range/);
wasmEval(toBuf(moduleWithSections([sigSection([v2vSig]), declSection([0]), tableSection([0]), bodySection([v2vBody])])));
wasmEval(toBuf(moduleWithSections([sigSection([v2vSig]), declSection([0]), tableSection([0,0]), bodySection([v2vBody])])));
assertErrorMessage(() => wasmEval(toBuf(moduleWithSections([sigSection([v2vSig]), declSection([0]), tableSection([0,1]), bodySection([v2vBody])]))), TypeError, /table element out of range/);
wasmEval(toBuf(moduleWithSections([sigSection([v2vSig]), declSection([0,0,0]), tableSection([0,1,0,2]), bodySection([v2vBody, v2vBody, v2vBody])])));
wasmEval(toBuf(moduleWithSections([sigSection([v2vSig,i2vSig]), declSection([0,0,1]), tableSection([0,1,2]), bodySection([v2vBody, v2vBody, v2vBody])])));
