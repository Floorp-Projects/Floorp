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

const VoidCode = 0;
const I32Code = 1;
const I64Code = 2;
const F32Code = 3;
const F64Code = 4;

function toU8(array) {
    for (let b of array)
        assertEq(b < 256, true);
    return Uint8Array.from(array);
}

function varU32(u32) {
    // TODO
    assertEq(u32 < 128, true);
    return [u32];
}

const U32MAX_LEB = [255, 255, 255, 255, 15];

const wasmEval = Wasm.instantiateModule;

assertErrorMessage(() => wasmEval(toU8([])), TypeError, magicError);
assertErrorMessage(() => wasmEval(toU8([42])), TypeError, magicError);
assertErrorMessage(() => wasmEval(toU8([magic0, magic1, magic2])), TypeError, magicError);
assertErrorMessage(() => wasmEval(toU8([1,2,3,4])), TypeError, magicError);
assertErrorMessage(() => wasmEval(toU8([magic0, magic1, magic2, magic3])), TypeError, versionError);
assertErrorMessage(() => wasmEval(toU8([magic0, magic1, magic2, magic3, 1])), TypeError, versionError);
assertErrorMessage(() => wasmEval(toU8([magic0, magic1, magic2, magic3, ver0])), TypeError, versionError);
assertErrorMessage(() => wasmEval(toU8([magic0, magic1, magic2, magic3, ver0, ver1, ver2])), TypeError, versionError);

function moduleHeaderThen(...rest) {
    return [magic0, magic1, magic2, magic3, ver0, ver1, ver2, ver3, ...rest];
}

var o = wasmEval(toU8(moduleHeaderThen()));
assertEq(Object.getOwnPropertyNames(o).length, 0);

wasmEval(toU8(moduleHeaderThen(1, 0)));        // unknown section containing 0-length string
wasmEval(toU8(moduleHeaderThen(2, 1, 0)));     // unknown section containing 1-length string ("\0")
wasmEval(toU8(moduleHeaderThen(1, 0,  1, 0)));
wasmEval(toU8(moduleHeaderThen(1, 0,  2, 1, 0)));
wasmEval(toU8(moduleHeaderThen(1, 0,  2, 1, 0)));

assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(1))), TypeError, sectionError);
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0, 1))), TypeError, sectionError);
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0, 0))), TypeError, unknownSectionError);

function cstring(name) {
    return (name + '\0').split('').map(c => c.charCodeAt(0));
}

function string(name) {
    var nameBytes = name.split('').map(c => {
        var code = c.charCodeAt(0);
        assertEq(code < 128, true); // TODO
        return code
    });
    return varU32(nameBytes.length).concat(nameBytes);
}

function moduleWithSections(sectionArray) {
    var bytes = moduleHeaderThen();
    for (let section of sectionArray) {
        var sectionName = string(section.name);
        bytes.push(...varU32(sectionName.length + section.body.length));
        bytes.push(...sectionName);
        bytes.push(...section.body);
    }
    return toU8(bytes);
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
        body.push(...string(imp.module));
        body.push(...string(imp.func));
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

assertErrorMessage(() => wasmEval(moduleWithSections([ {name: sigId, body: U32MAX_LEB } ])), TypeError, /too many signatures/);
assertErrorMessage(() => wasmEval(moduleWithSections([ {name: sigId, body: [1, ...U32MAX_LEB], } ])), TypeError, /too many arguments in signature/);
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([{args:[], ret:VoidCode}, {args:[], ret:VoidCode}])])), TypeError, /duplicate signature/);

assertThrowsInstanceOf(() => wasmEval(moduleWithSections([{name: sigId, body: [1]}])), TypeError);
assertThrowsInstanceOf(() => wasmEval(moduleWithSections([{name: sigId, body: [1, 1, 0]}])), TypeError);

wasmEval(moduleWithSections([sigSection([])]));
wasmEval(moduleWithSections([sigSection([v2vSig])]));
wasmEval(moduleWithSections([sigSection([i2vSig])]));
wasmEval(moduleWithSections([sigSection([v2vSig, i2vSig])]));

assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([{args:[], ret:100}])])), TypeError, /expression type/);
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([{args:[100], ret:VoidCode}])])), TypeError, /value type/);

assertThrowsInstanceOf(() => wasmEval(moduleWithSections([sigSection([]), declSection([0])])), TypeError, /signature index out of range/);
assertThrowsInstanceOf(() => wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([1])])), TypeError, /signature index out of range/);
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0])])), TypeError, /expected function bodies/);
wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), bodySection([v2vBody])]));

assertThrowsInstanceOf(() => wasmEval(moduleWithSections([sigSection([v2vSig]), {name: importId, body:[]}])), TypeError);
assertErrorMessage(() => wasmEval(moduleWithSections([importSection([{sigIndex:0, module:"a", func:"b"}])])), TypeError, /signature index out of range/);
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([v2vSig]), importSection([{sigIndex:1, module:"a", func:"b"}])])), TypeError, /signature index out of range/);
wasmEval(moduleWithSections([sigSection([v2vSig]), importSection([])]));
wasmEval(moduleWithSections([sigSection([v2vSig]), importSection([{sigIndex:0, module:"a", func:""}])]), {a:()=>{}});

wasmEval(moduleWithSections([
    sigSection([v2vSig]),
    importSection([{sigIndex:0, module:"a", func:""}]),
    declSection([0]),
    bodySection([v2vBody])
]), {a:()=>{}});

assertErrorMessage(() => wasmEval(moduleWithSections([ {name: dataSegmentsId, body: [], } ])), TypeError, /data section requires a memory section/);

wasmEval(moduleWithSections([tableSection([])]));
assertErrorMessage(() => wasmEval(moduleWithSections([tableSection([0])])), TypeError, /table element out of range/);
wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), tableSection([0]), bodySection([v2vBody])]));
wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), tableSection([0,0]), bodySection([v2vBody])]));
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), tableSection([0,1]), bodySection([v2vBody])])), TypeError, /table element out of range/);
wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0,0,0]), tableSection([0,1,0,2]), bodySection([v2vBody, v2vBody, v2vBody])]));
wasmEval(moduleWithSections([sigSection([v2vSig,i2vSig]), declSection([0,0,1]), tableSection([0,1,2]), bodySection([v2vBody, v2vBody, v2vBody])]));
