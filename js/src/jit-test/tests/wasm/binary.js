load(libdir + "wasm.js");

// MagicNumber = 0x6d736100;
const magic0 = 0x00;  // '\0'
const magic1 = 0x61;  // 'a'
const magic2 = 0x73;  // 's'
const magic3 = 0x6d;  // 'm'

// EncodingVersion (temporary; to be set to 1 at some point before release)
const ver0 = (Wasm.experimentalVersion >>>  0) & 0xff;
const ver1 = (Wasm.experimentalVersion >>>  8) & 0xff;
const ver2 = (Wasm.experimentalVersion >>> 16) & 0xff;
const ver3 = (Wasm.experimentalVersion >>> 24) & 0xff;

// Section names
const sigId                = "type";
const importId             = "import";
const functionSignaturesId = "function";
const functionTableId      = "table";
const exportTableId        = "export";
const functionBodiesId     = "code";
const dataSegmentsId       = "data";
const nameId               = "name";

const magicError = /failed to match magic number/;
const versionError = /failed to match binary version/;
const unknownSectionError = /failed to skip unknown section at end/;
const sectionError = /failed to start section/;

const VoidCode = 0;
const I32Code = 1;
const I64Code = 2;
const F32Code = 3;
const F64Code = 4;

const FunctionConstructorCode = 0x40;

const Block = 0x01;
const End = 0x0f;
const CallImport = 0x18;

function toU8(array) {
    for (let b of array)
        assertEq(b < 256, true);
    return Uint8Array.from(array);
}

function varU32(u32) {
    assertEq(u32 >= 0, true);
    assertEq(u32 < Math.pow(2,32), true);
    var bytes = [];
    do {
        var byte = u32 & 0x7f;
        u32 >>>= 7;
        if (u32 != 0)
            byte |= 0x80;
        bytes.push(byte);
    } while (u32 != 0);
    return bytes;
}

const U32MAX_LEB = [255, 255, 255, 255, 15];

const wasmEval = (code, imports) => Wasm.instantiateModule(code, imports).exports;

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

wasmEval(toU8(moduleHeaderThen(0, 0)));        // unknown section containing 0-length string
wasmEval(toU8(moduleHeaderThen(1, 0, 0)));     // unknown section containing 1-length string ("\0")
wasmEval(toU8(moduleHeaderThen(0, 0,  0, 0)));
wasmEval(toU8(moduleHeaderThen(0, 0,  1, 0, 0)));
wasmEval(toU8(moduleHeaderThen(1, 0, 0,  1, 0, 0)));
wasmEval(toU8(moduleHeaderThen(1, 0, 0,  0, 0)));

assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(1))), TypeError, sectionError);
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0, 1))), TypeError, unknownSectionError);

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

function encodedString(name, len) {
    var nameBytes = name.split('').map(c => c.charCodeAt(0));
    return varU32(len === undefined ? nameBytes.length : len).concat(nameBytes);
}

function moduleWithSections(sectionArray) {
    var bytes = moduleHeaderThen();
    for (let section of sectionArray) {
        bytes.push(...string(section.name));
        bytes.push(...varU32(section.body.length));
        bytes.push(...section.body);
    }
    return toU8(bytes);
}

function sigSection(sigs) {
    var body = [];
    body.push(...varU32(sigs.length));
    for (let sig of sigs) {
        body.push(...varU32(FunctionConstructorCode));
        body.push(...varU32(sig.args.length));
        for (let arg of sig.args)
            body.push(...varU32(arg));
        body.push(...varU32(sig.ret == VoidCode ? 0 : 1));
        if (sig.ret != VoidCode)
            body.push(...varU32(sig.ret));
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
    var body = varU32(bodies.length).concat(...bodies);
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

function exportSection(exports) {
    var body = [];
    body.push(...varU32(exports.length));
    for (let exp of exports) {
        body.push(...varU32(exp.funcIndex));
        body.push(...string(exp.name));
    }
    return { name: exportTableId, body };
}

function tableSection(elems) {
    var body = [];
    body.push(...varU32(elems.length));
    for (let i of elems)
        body.push(...varU32(i));
    return { name: functionTableId, body };
}

function nameSection(elems) {
    var body = [];
    body.push(...varU32(elems.length));
    for (let fn of elems) {
        body.push(...encodedString(fn.name, fn.nameLen));
        if (!fn.locals) {
           body.push(...varU32(0));
           continue;
        }
        body.push(...varU32(fn.locals.length));
        for (let local of fn.locals)
            body.push(...encodedString(local.name, local.nameLen));
    }
    return { name: nameId, body };
}

const v2vSig = {args:[], ret:VoidCode};
const i2vSig = {args:[I32Code], ret:VoidCode};
const v2vBody = funcBody({locals:[], body:[]});

assertErrorMessage(() => wasmEval(moduleWithSections([ {name: sigId, body: U32MAX_LEB } ])), TypeError, /too many signatures/);
assertErrorMessage(() => wasmEval(moduleWithSections([ {name: sigId, body: [1, 0], } ])), TypeError, /expected function form/);
assertErrorMessage(() => wasmEval(moduleWithSections([ {name: sigId, body: [1, FunctionConstructorCode, ...U32MAX_LEB], } ])), TypeError, /too many arguments in signature/);

assertThrowsInstanceOf(() => wasmEval(moduleWithSections([{name: sigId, body: [1]}])), TypeError);
assertThrowsInstanceOf(() => wasmEval(moduleWithSections([{name: sigId, body: [1, 1, 0]}])), TypeError);

wasmEval(moduleWithSections([sigSection([])]));
wasmEval(moduleWithSections([sigSection([v2vSig])]));
wasmEval(moduleWithSections([sigSection([i2vSig])]));
wasmEval(moduleWithSections([sigSection([v2vSig, i2vSig])]));

assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([{args:[], ret:100}])])), TypeError, /bad type/);
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([{args:[100], ret:VoidCode}])])), TypeError, /bad type/);

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

// Deep nesting shouldn't crash or even throw.
var manyBlocks = [];
for (var i = 0; i < 20000; i++)
    manyBlocks.push(Block, End);
wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), bodySection([funcBody({locals:[], body:manyBlocks})])]));

// Checking stack trace.
function runStartTraceTest(namesContent, expectedName) {
    var sections = [
        sigSection([v2vSig]),
        importSection([{sigIndex:0, module:"env", func:"callback"}]),
        declSection([0]),
        exportSection([{funcIndex:0, name: "run"}]),
        bodySection([funcBody({locals: [], body: [CallImport, varU32(0), varU32(0)]})])
    ];
    if (namesContent)
        sections.push(nameSection(namesContent));
    var result = "";
    var callback = () => {
        var prevFrameEntry = new Error().stack.split('\n')[1];
        result = prevFrameEntry.split('@')[0];
    };
    wasmEval(moduleWithSections(sections), {"env": { callback }}).run();
    assertEq(result, expectedName);
};

runStartTraceTest(null, 'wasm-function[0]');
runStartTraceTest([{name: 'test'}], 'test');
runStartTraceTest([{name: 'test', locals: [{name: 'var1'}, {name: 'var2'}]}], 'test');
runStartTraceTest([{name: 'test', locals: [{name: 'var1'}, {name: 'var2'}]}], 'test');
runStartTraceTest([{name: 'test1'}, {name: 'test2'}], 'test1');
runStartTraceTest([], 'wasm-function[0]');
// Notice that invalid names section content shall not fail the parsing
runStartTraceTest([{nameLen: 100, name: 'test'}], 'wasm-function[0]'); // invalid name size
runStartTraceTest([{name: 'test', locals: [{nameLen: 40, name: 'var1'}]}], 'wasm-function[0]'); // invalid variable name size
runStartTraceTest([{name: ''}], 'wasm-function[0]'); // empty name
runStartTraceTest([{name: 'te\xE0\xFF'}], 'wasm-function[0]'); // invalid UTF8 name
