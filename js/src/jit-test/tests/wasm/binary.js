// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

const CompileError = WebAssembly.CompileError;

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

// Section opcodes
const userDefinedId = 0;
const typeId = 1;
const importId = 2;
const functionId = 3;
const tableId = 4;
const memoryId = 5;
const globalId = 6;
const exportId = 7;
const startId = 8;
const elemId = 9;
const codeId = 10;
const dataId = 11;

const nameName = "name";

const magicError = /failed to match magic number/;
const unknownSection = /expected user-defined section/;

function sectionError(section) {
    return RegExp(`failed to start ${section} section`);
}

function versionError(actual) {
    var expect = Wasm.experimentalVersion;
    var str = `binary version 0x${actual.toString(16)} does not match expected version 0x${expect.toString(16)}`;
    return RegExp(str);
}

const VoidCode = 0;
const I32Code = 1;
const I64Code = 2;
const F32Code = 3;
const F64Code = 4;

const AnyFuncCode = 0x20;
const FunctionConstructorCode = 0x40;

const Unreachable = 0x00;
const Block       = 0x01;
const End         = 0x0f;
const I32Const    = 0x10;
const I64Const    = 0x11;
const F64Const    = 0x12;
const F32Const    = 0x13;
const Call        = 0x16;

// DefinitionKind
const FunctionCode = 0x00;
const TableCode    = 0x01;
const MemoryCode   = 0x02;
const GlobalCode   = 0x03;

// ResizableFlags
const DefaultFlag    = 0x1;
const HasMaximumFlag = 0x2;

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

function varS32(s32) {
    assertEq(s32 >= -Math.pow(2,31), true);
    assertEq(s32 < Math.pow(2,31), true);
    var bytes = [];
    do {
        var byte = s32 & 0x7f;
        s32 >>= 7;
        if (s32 != 0 && s32 != -1)
            byte |= 0x80;
        bytes.push(byte);
    } while (s32 != 0 && s32 != -1);
    return bytes;
}

const U32MAX_LEB = [255, 255, 255, 255, 15];

const wasmEval = (code, imports) => new WebAssembly.Instance(new WebAssembly.Module(code), imports).exports;

assertErrorMessage(() => wasmEval(toU8([])), CompileError, magicError);
assertErrorMessage(() => wasmEval(toU8([42])), CompileError, magicError);
assertErrorMessage(() => wasmEval(toU8([magic0, magic1, magic2])), CompileError, magicError);
assertErrorMessage(() => wasmEval(toU8([1,2,3,4])), CompileError, magicError);
assertErrorMessage(() => wasmEval(toU8([magic0, magic1, magic2, magic3])), CompileError, versionError(0x6d736100));
assertErrorMessage(() => wasmEval(toU8([magic0, magic1, magic2, magic3, 1])), CompileError, versionError(0x6d736100));
assertErrorMessage(() => wasmEval(toU8([magic0, magic1, magic2, magic3, ver0])), CompileError, versionError(0x6d736100));
assertErrorMessage(() => wasmEval(toU8([magic0, magic1, magic2, magic3, ver0, ver1, ver2])), CompileError, versionError(0x6d736100));

function moduleHeaderThen(...rest) {
    return [magic0, magic1, magic2, magic3, ver0, ver1, ver2, ver3, ...rest];
}

var o = wasmEval(toU8(moduleHeaderThen()));
assertEq(Object.getOwnPropertyNames(o).length, 0);

// unfinished known sections
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(typeId))), CompileError, sectionError("type"));
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(importId))), CompileError, sectionError("import"));
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(functionId))), CompileError, sectionError("function"));
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(tableId))), CompileError, sectionError("table"));
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(memoryId))), CompileError, sectionError("memory"));
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(globalId))), CompileError, sectionError("global"));
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(exportId))), CompileError, sectionError("export"));
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(startId))), CompileError, sectionError("start"));
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(elemId))), CompileError, sectionError("elem"));
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(codeId))), CompileError, sectionError("code"));
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(dataId))), CompileError, sectionError("data"));

// unknown sections are unconditionally rejected
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(42))), CompileError, unknownSection);
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(42, 0))), CompileError, unknownSection);
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(42, 1, 0))), CompileError, unknownSection);

// user sections have special rules
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0))), CompileError, sectionError("user-defined"));  // no length
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0, 0))), CompileError, sectionError("user-defined"));  // no id
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0, 0, 0))), CompileError, sectionError("user-defined"));  // payload too small to have id length
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0, 1, 1))), CompileError, sectionError("user-defined"));  // id not present
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0, 1, 1, 65))), CompileError, sectionError("user-defined"));  // id length doesn't fit in section
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0, 1, 0, 0))), CompileError, sectionError("user-defined"));  // second, unfinished user-defined section
wasmEval(toU8(moduleHeaderThen(0, 1, 0)));  // empty id
wasmEval(toU8(moduleHeaderThen(0, 1, 0,  0, 1, 0)));  // 2x empty id
wasmEval(toU8(moduleHeaderThen(0, 2, 1, 65)));  // id = "A"

function string(name) {
    var nameBytes = name.split('').map(c => {
        var code = c.charCodeAt(0);
        assertEq(code < 128, true); // TODO
        return code
    });
    return varU32(nameBytes.length).concat(nameBytes);
}

function encodedString(name, len) {
    var name = unescape(encodeURIComponent(name)); // break into string of utf8 code points
    var nameBytes = name.split('').map(c => c.charCodeAt(0)); // map to array of numbers
    return varU32(len === undefined ? nameBytes.length : len).concat(nameBytes);
}

function moduleWithSections(sectionArray) {
    var bytes = moduleHeaderThen();
    for (let section of sectionArray) {
        bytes.push(section.name);
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
    return { name: typeId, body };
}

function declSection(decls) {
    var body = [];
    body.push(...varU32(decls.length));
    for (let decl of decls)
        body.push(...varU32(decl));
    return { name: functionId, body };
}

function funcBody(func) {
    var body = varU32(func.locals.length);
    for (let local of func.locals)
        body.push(...varU32(local));
    body = body.concat(...func.body);
    body.push(End);
    body.splice(0, 0, ...varU32(body.length));
    return body;
}

function bodySection(bodies) {
    var body = varU32(bodies.length).concat(...bodies);
    return { name: codeId, body };
}

function importSection(imports) {
    var body = [];
    body.push(...varU32(imports.length));
    for (let imp of imports) {
        body.push(...string(imp.module));
        body.push(...string(imp.func));
        body.push(...varU32(FunctionCode));
        body.push(...varU32(imp.sigIndex));
    }
    return { name: importId, body };
}

function exportSection(exports) {
    var body = [];
    body.push(...varU32(exports.length));
    for (let exp of exports) {
        body.push(...string(exp.name));
        body.push(...varU32(FunctionCode));
        body.push(...varU32(exp.funcIndex));
    }
    return { name: exportId, body };
}

function tableSection(initialSize) {
    var body = [];
    body.push(...varU32(1));           // number of tables
    body.push(...varU32(AnyFuncCode));
    body.push(...varU32(DefaultFlag)); // for now, no maximum
    body.push(...varU32(initialSize));
    // for now, no maximum
    return { name: tableId, body };
}

function memorySection(initialSize) {
    var body = [];
    body.push(...varU32(1));           // number of memories
    body.push(...varU32(DefaultFlag)); // for now, no maximum
    body.push(...varU32(initialSize));
    // for now, no maximum
    return { name: memoryId, body };
}

function elemSection(elemArrays) {
    var body = [];
    body.push(...varU32(elemArrays.length));
    for (let array of elemArrays) {
        body.push(...varU32(0)); // table index
        body.push(...varU32(I32Const));
        body.push(...varS32(array.offset));
        body.push(...varU32(End));
        body.push(...varU32(array.elems.length));
        for (let elem of array.elems)
            body.push(...varU32(elem));
    }
    return { name: elemId, body };
}

function nameSection(elems) {
    var body = [];
    body.push(...string(nameName));
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
    return { name: userDefinedId, body };
}

function userDefinedSection(name, ...body) {
    return { name: userDefinedId, body: [...string(name), ...body] };
}

const v2vSig = {args:[], ret:VoidCode};
const i2vSig = {args:[I32Code], ret:VoidCode};
const v2vBody = funcBody({locals:[], body:[]});

assertErrorMessage(() => wasmEval(moduleWithSections([ {name: typeId, body: U32MAX_LEB } ])), CompileError, /too many signatures/);
assertErrorMessage(() => wasmEval(moduleWithSections([ {name: typeId, body: [1, 0], } ])), CompileError, /expected function form/);
assertErrorMessage(() => wasmEval(moduleWithSections([ {name: typeId, body: [1, FunctionConstructorCode, ...U32MAX_LEB], } ])), CompileError, /too many arguments in signature/);

assertThrowsInstanceOf(() => wasmEval(moduleWithSections([{name: typeId, body: [1]}])), CompileError);
assertThrowsInstanceOf(() => wasmEval(moduleWithSections([{name: typeId, body: [1, 1, 0]}])), CompileError);

wasmEval(moduleWithSections([sigSection([])]));
wasmEval(moduleWithSections([sigSection([v2vSig])]));
wasmEval(moduleWithSections([sigSection([i2vSig])]));
wasmEval(moduleWithSections([sigSection([v2vSig, i2vSig])]));

assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([{args:[], ret:100}])])), CompileError, /bad type/);
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([{args:[100], ret:VoidCode}])])), CompileError, /bad type/);

assertThrowsInstanceOf(() => wasmEval(moduleWithSections([sigSection([]), declSection([0])])), CompileError, /signature index out of range/);
assertThrowsInstanceOf(() => wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([1])])), CompileError, /signature index out of range/);
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0])])), CompileError, /expected function bodies/);
wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), bodySection([v2vBody])]));

assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), bodySection([v2vBody.concat(v2vBody)])])), CompileError, /byte size mismatch in code section/);

assertThrowsInstanceOf(() => wasmEval(moduleWithSections([sigSection([v2vSig]), {name: importId, body:[]}])), CompileError);
assertErrorMessage(() => wasmEval(moduleWithSections([importSection([{sigIndex:0, module:"a", func:"b"}])])), CompileError, /signature index out of range/);
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([v2vSig]), importSection([{sigIndex:1, module:"a", func:"b"}])])), CompileError, /signature index out of range/);
wasmEval(moduleWithSections([sigSection([v2vSig]), importSection([])]));
wasmEval(moduleWithSections([sigSection([v2vSig]), importSection([{sigIndex:0, module:"a", func:""}])]), {a:{"":()=>{}}});

wasmEval(moduleWithSections([
    sigSection([v2vSig]),
    importSection([{sigIndex:0, module:"a", func:""}]),
    declSection([0]),
    bodySection([v2vBody])
]), {a:{"":()=>{}}});

assertErrorMessage(() => wasmEval(moduleWithSections([ {name: dataId, body: [], } ])), CompileError, /data section requires a memory section/);

wasmEval(moduleWithSections([tableSection(0)]));
wasmEval(moduleWithSections([elemSection([])]));
wasmEval(moduleWithSections([tableSection(0), elemSection([])]));
wasmEval(moduleWithSections([tableSection(1), elemSection([{offset:1, elems:[]}])]));
assertErrorMessage(() => wasmEval(moduleWithSections([tableSection(1), elemSection([{offset:0, elems:[0]}])])), CompileError, /table element out of range/);
assertErrorMessage(() => wasmEval(moduleWithSections([tableSection(0), elemSection([{offset:0, elems:[0]}])])), CompileError, /element segment does not fit/);
assertErrorMessage(() => wasmEval(moduleWithSections([tableSection(1), elemSection([{offset:1, elems:[0]}])])), CompileError, /element segment does not fit/);
assertErrorMessage(() => wasmEval(moduleWithSections([tableSection(1), elemSection([{offset:0, elems:[0,0]}])])), CompileError, /element segment does not fit/);
assertErrorMessage(() => wasmEval(moduleWithSections([tableSection(0), elemSection([{offset:1, elems:[]}])])), CompileError, /element segment does not fit/);
assertErrorMessage(() => wasmEval(moduleWithSections([tableSection(0), elemSection([{offset:-1, elems:[]}])])), CompileError, /element segment does not fit/);
wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), tableSection(1), elemSection([{offset:0, elems:[0]}]), bodySection([v2vBody])]));
wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), tableSection(2), elemSection([{offset:0, elems:[0,0]}]), bodySection([v2vBody])]));
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), tableSection(2), elemSection([{offset:0, elems:[0,1]}]), bodySection([v2vBody])])), CompileError, /table element out of range/);
wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0,0,0]), tableSection(4), elemSection([{offset:0, elems:[0,1,0,2]}]), bodySection([v2vBody, v2vBody, v2vBody])]));
wasmEval(moduleWithSections([sigSection([v2vSig,i2vSig]), declSection([0,0,1]), tableSection(3), elemSection([{offset:0,elems:[0,1,2]}]), bodySection([v2vBody, v2vBody, v2vBody])]));

function invalidTableSection0() {
    var body = [];
    body.push(...varU32(0));           // number of tables
    return { name: tableId, body };
}

assertErrorMessage(() => wasmEval(moduleWithSections([invalidTableSection0()])), CompileError, /number of tables must be exactly one/);

wasmEval(moduleWithSections([memorySection(0)]));

function invalidMemorySection0() {
    var body = [];
    body.push(...varU32(0));           // number of memories
    return { name: memoryId, body };
}

assertErrorMessage(() => wasmEval(moduleWithSections([invalidMemorySection0()])), CompileError, /number of memories must be exactly one/);

// Test early 'end'
const bodyMismatch = /function body length mismatch/;
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), bodySection([funcBody({locals:[], body:[End]})])])), CompileError, bodyMismatch);
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), bodySection([funcBody({locals:[], body:[Unreachable,End]})])])), CompileError, bodyMismatch);
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), bodySection([funcBody({locals:[], body:[End,Unreachable]})])])), CompileError, bodyMismatch);

// Deep nesting shouldn't crash or even throw.
var manyBlocks = [];
for (var i = 0; i < 20000; i++)
    manyBlocks.push(Block, VoidCode, End);
wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), bodySection([funcBody({locals:[], body:manyBlocks})])]));

// Ignore errors in name section.
var tooBigNameSection = {
    name: userDefinedId,
    body: [...string(nameName), ...varU32(Math.pow(2, 31))] // declare 2**31 functions.
};
wasmEval(moduleWithSections([tooBigNameSection]));

// Skip user-defined sections before any expected section
var userDefSec = userDefinedSection("wee", 42, 13);
var sigSec = sigSection([v2vSig]);
var declSec = declSection([0]);
var bodySec = bodySection([v2vBody]);
wasmEval(moduleWithSections([userDefSec, sigSec, declSec, bodySec]));
wasmEval(moduleWithSections([sigSec, userDefSec, declSec, bodySec]));
wasmEval(moduleWithSections([sigSec, declSec, userDefSec, bodySec]));
wasmEval(moduleWithSections([sigSec, declSec, bodySec, userDefSec]));
wasmEval(moduleWithSections([userDefSec, userDefSec, sigSec, declSec, bodySec]));
wasmEval(moduleWithSections([userDefSec, userDefSec, sigSec, userDefSec, declSec, userDefSec, bodySec]));

// Diagnose nonstandard block signature types.
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), bodySection([funcBody({locals:[], body:[Block, F64Code + 1, End]})])])), CompileError, /unknown block signature type/);

// Checking stack trace.
function runStackTraceTest(namesContent, expectedName) {
    var sections = [
        sigSection([v2vSig]),
        importSection([{sigIndex:0, module:"env", func:"callback"}]),
        declSection([0]),
        exportSection([{funcIndex:1, name: "run"}]),
        bodySection([funcBody({locals: [], body: [Call, varU32(0)]})]),
        userDefinedSection("whoa"),
        userDefinedSection("wee", 42),
    ];
    if (namesContent)
        sections.push(nameSection(namesContent));
    sections.push(userDefinedSection("yay", 13));

    var result = "";
    var callback = () => {
        var prevFrameEntry = new Error().stack.split('\n')[1];
        result = prevFrameEntry.split('@')[0];
    };
    wasmEval(moduleWithSections(sections), {"env": { callback }}).run();
    assertEq(result, expectedName);
};

runStackTraceTest(null, 'wasm-function[0]');
runStackTraceTest([{name: 'test'}], 'test');
runStackTraceTest([{name: 'test', locals: [{name: 'var1'}, {name: 'var2'}]}], 'test');
runStackTraceTest([{name: 'test', locals: [{name: 'var1'}, {name: 'var2'}]}], 'test');
runStackTraceTest([{name: 'test1'}, {name: 'test2'}], 'test1');
runStackTraceTest([{name: 'test☃'}], 'test☃');
runStackTraceTest([{name: 'te\xE0\xFF'}], 'te\xE0\xFF');
runStackTraceTest([], 'wasm-function[0]');
// Notice that invalid names section content shall not fail the parsing
runStackTraceTest([{nameLen: 100, name: 'test'}], 'wasm-function[0]'); // invalid name size
runStackTraceTest([{name: 'test', locals: [{nameLen: 40, name: 'var1'}]}], 'wasm-function[0]'); // invalid variable name size
runStackTraceTest([{name: ''}], 'wasm-function[0]'); // empty name
