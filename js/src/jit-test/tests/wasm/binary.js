load(libdir + "wasm-binary.js");

const Module = WebAssembly.Module;
const CompileError = WebAssembly.CompileError;

const magicError = /failed to match magic number/;
const unknownSection = /expected custom section/;

function sectionError(section) {
    return RegExp(`failed to start ${section} section`);
}

function versionError(actual) {
    var expect = encodingVersion;
    var str = `binary version 0x${actual.toString(16)} does not match expected version 0x${expect.toString(16)}`;
    return RegExp(str);
}

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

const wasmEval = (code, imports) => new WebAssembly.Instance(new Module(code), imports).exports;

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
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0))), CompileError, sectionError("custom"));  // no length
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0, 0))), CompileError, sectionError("custom"));  // no id
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0, 0, 0))), CompileError, sectionError("custom"));  // payload too small to have id length
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0, 1, 1))), CompileError, sectionError("custom"));  // id not present
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0, 1, 1, 65))), CompileError, sectionError("custom"));  // id length doesn't fit in section
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(0, 1, 0, 0))), CompileError, sectionError("custom"));  // second, unfinished custom section
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
        body.push(...varU32(FuncCode));
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
    body.push(EndCode);
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
    body.push(...varU32(0x0));         // for now, no maximum
    body.push(...varU32(initialSize));
    return { name: tableId, body };
}

function memorySection(initialSize) {
    var body = [];
    body.push(...varU32(1));           // number of memories
    body.push(...varU32(0x0));         // for now, no maximum
    body.push(...varU32(initialSize));
    return { name: memoryId, body };
}

function dataSection(segmentArrays) {
    var body = [];
    body.push(...varU32(segmentArrays.length));
    for (let array of segmentArrays) {
        body.push(...varU32(0)); // table index
        body.push(...varU32(I32ConstCode));
        body.push(...varS32(array.offset));
        body.push(...varU32(EndCode));
        body.push(...varU32(array.elems.length));
        for (let elem of array.elems)
            body.push(...varU32(elem));
    }
    return { name: dataId, body };
}

function elemSection(elemArrays) {
    var body = [];
    body.push(...varU32(elemArrays.length));
    for (let array of elemArrays) {
        body.push(...varU32(0)); // table index
        body.push(...varU32(I32ConstCode));
        body.push(...varS32(array.offset));
        body.push(...varU32(EndCode));
        body.push(...varU32(array.elems.length));
        for (let elem of array.elems)
            body.push(...varU32(elem));
    }
    return { name: elemId, body };
}

function nameSection(moduleName, funcNames) {
    var body = [];
    body.push(...string(nameName));

    if (moduleName) {
        body.push(...varU32(nameTypeModule));

        var subsection = encodedString(moduleName);

        body.push(...varU32(subsection.length));
        body.push(...subsection);
    }

    if (funcNames) {
        body.push(...varU32(nameTypeFunction));

        var subsection = varU32(funcNames.length);

        var funcIndex = 0;
        for (let f of funcNames) {
            subsection.push(...varU32(f.index ? f.index : funcIndex));
            subsection.push(...encodedString(f.name, f.nameLen));
            funcIndex++;
        }

        body.push(...varU32(subsection.length));
        body.push(...subsection);
    }

    return { name: userDefinedId, body };
}

function customSection(name, ...body) {
    return { name: userDefinedId, body: [...string(name), ...body] };
}

const v2vSig = {args:[], ret:VoidCode};
const v2vSigSection = sigSection([v2vSig]);
const i2vSig = {args:[I32Code], ret:VoidCode};
const v2vBody = funcBody({locals:[], body:[]});

assertErrorMessage(() => wasmEval(moduleWithSections([ {name: typeId, body: U32MAX_LEB } ])), CompileError, /too many signatures/);
assertErrorMessage(() => wasmEval(moduleWithSections([ {name: typeId, body: [1, 0], } ])), CompileError, /expected function form/);
assertErrorMessage(() => wasmEval(moduleWithSections([ {name: typeId, body: [1, FuncCode, ...U32MAX_LEB], } ])), CompileError, /too many arguments in signature/);

assertThrowsInstanceOf(() => wasmEval(moduleWithSections([{name: typeId, body: [1]}])), CompileError);
assertThrowsInstanceOf(() => wasmEval(moduleWithSections([{name: typeId, body: [1, 1, 0]}])), CompileError);

wasmEval(moduleWithSections([sigSection([])]));
wasmEval(moduleWithSections([v2vSigSection]));
wasmEval(moduleWithSections([sigSection([i2vSig])]));
wasmEval(moduleWithSections([sigSection([v2vSig, i2vSig])]));

assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([{args:[], ret:100}])])), CompileError, /bad type/);
assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([{args:[100], ret:VoidCode}])])), CompileError, /bad type/);

assertThrowsInstanceOf(() => wasmEval(moduleWithSections([sigSection([]), declSection([0])])), CompileError, /signature index out of range/);
assertThrowsInstanceOf(() => wasmEval(moduleWithSections([v2vSigSection, declSection([1])])), CompileError, /signature index out of range/);
assertErrorMessage(() => wasmEval(moduleWithSections([v2vSigSection, declSection([0])])), CompileError, /expected function bodies/);
wasmEval(moduleWithSections([v2vSigSection, declSection([0]), bodySection([v2vBody])]));

assertErrorMessage(() => wasmEval(moduleWithSections([v2vSigSection, declSection([0]), bodySection([v2vBody.concat(v2vBody)])])), CompileError, /byte size mismatch in code section/);

assertThrowsInstanceOf(() => wasmEval(moduleWithSections([v2vSigSection, {name: importId, body:[]}])), CompileError);
assertErrorMessage(() => wasmEval(moduleWithSections([importSection([{sigIndex:0, module:"a", func:"b"}])])), CompileError, /signature index out of range/);
assertErrorMessage(() => wasmEval(moduleWithSections([v2vSigSection, importSection([{sigIndex:1, module:"a", func:"b"}])])), CompileError, /signature index out of range/);
wasmEval(moduleWithSections([v2vSigSection, importSection([])]));
wasmEval(moduleWithSections([v2vSigSection, importSection([{sigIndex:0, module:"a", func:""}])]), {a:{"":()=>{}}});

wasmEval(moduleWithSections([
    v2vSigSection,
    importSection([{sigIndex:0, module:"a", func:""}]),
    declSection([0]),
    bodySection([v2vBody])
]), {a:{"":()=>{}}});

assertErrorMessage(() => wasmEval(moduleWithSections([ dataSection([{offset:1, elems:[]}]) ])), CompileError, /data segment requires a memory section/);

wasmEval(moduleWithSections([tableSection(0)]));
wasmEval(moduleWithSections([elemSection([])]));
wasmEval(moduleWithSections([tableSection(0), elemSection([])]));
wasmEval(moduleWithSections([tableSection(1), elemSection([{offset:1, elems:[]}])]));
assertErrorMessage(() => wasmEval(moduleWithSections([tableSection(1), elemSection([{offset:0, elems:[0]}])])), CompileError, /table element out of range/);
wasmEval(moduleWithSections([v2vSigSection, declSection([0]), tableSection(1), elemSection([{offset:0, elems:[0]}]), bodySection([v2vBody])]));
wasmEval(moduleWithSections([v2vSigSection, declSection([0]), tableSection(2), elemSection([{offset:0, elems:[0,0]}]), bodySection([v2vBody])]));
assertErrorMessage(() => wasmEval(moduleWithSections([v2vSigSection, declSection([0]), tableSection(2), elemSection([{offset:0, elems:[0,1]}]), bodySection([v2vBody])])), CompileError, /table element out of range/);
wasmEval(moduleWithSections([v2vSigSection, declSection([0,0,0]), tableSection(4), elemSection([{offset:0, elems:[0,1,0,2]}]), bodySection([v2vBody, v2vBody, v2vBody])]));
wasmEval(moduleWithSections([sigSection([v2vSig,i2vSig]), declSection([0,0,1]), tableSection(3), elemSection([{offset:0,elems:[0,1,2]}]), bodySection([v2vBody, v2vBody, v2vBody])]));

function tableSection0() {
    var body = [];
    body.push(...varU32(0));           // number of tables
    return { name: tableId, body };
}

function invalidTableSection2() {
    var body = [];
    body.push(...varU32(2));           // number of tables
    body.push(...varU32(AnyFuncCode));
    body.push(...varU32(0x0));
    body.push(...varU32(0));
    body.push(...varU32(AnyFuncCode));
    body.push(...varU32(0x0));
    body.push(...varU32(0));
    return { name: tableId, body };
}

wasmEval(moduleWithSections([tableSection0()]));
assertErrorMessage(() => wasmEval(moduleWithSections([invalidTableSection2()])), CompileError, /number of tables must be at most one/);

wasmEval(moduleWithSections([memorySection(0)]));

function memorySection0() {
    var body = [];
    body.push(...varU32(0));           // number of memories
    return { name: memoryId, body };
}

function invalidMemorySection2() {
    var body = [];
    body.push(...varU32(2));           // number of memories
    body.push(...varU32(0x0));
    body.push(...varU32(0));
    body.push(...varU32(0x0));
    body.push(...varU32(0));
    return { name: memoryId, body };
}

wasmEval(moduleWithSections([memorySection0()]));
assertErrorMessage(() => wasmEval(moduleWithSections([invalidMemorySection2()])), CompileError, /number of memories must be at most one/);

// Test early 'end'
const bodyMismatch = /function body length mismatch/;
assertErrorMessage(() => wasmEval(moduleWithSections([v2vSigSection, declSection([0]), bodySection([funcBody({locals:[], body:[EndCode]})])])), CompileError, bodyMismatch);
assertErrorMessage(() => wasmEval(moduleWithSections([v2vSigSection, declSection([0]), bodySection([funcBody({locals:[], body:[UnreachableCode,EndCode]})])])), CompileError, bodyMismatch);
assertErrorMessage(() => wasmEval(moduleWithSections([v2vSigSection, declSection([0]), bodySection([funcBody({locals:[], body:[EndCode,UnreachableCode]})])])), CompileError, bodyMismatch);

// Deep nesting shouldn't crash or even throw. This test takes a long time to
// run with the JITs disabled, so to avoid occasional timeout, disable. Also
// in eager mode, this triggers pathological recompilation, so only run for
// "normal" JIT modes. This test is totally independent of the JITs so this
// shouldn't matter.
var jco = getJitCompilerOptions();
if (jco["ion.enable"] && jco["baseline.enable"] && jco["baseline.warmup.trigger"] > 0 && jco["ion.warmup.trigger"] > 10) {
    var manyBlocks = [];
    for (var i = 0; i < 20000; i++)
        manyBlocks.push(BlockCode, VoidCode);
    for (var i = 0; i < 20000; i++)
        manyBlocks.push(EndCode);
    wasmEval(moduleWithSections([v2vSigSection, declSection([0]), bodySection([funcBody({locals:[], body:manyBlocks})])]));
}

// Ignore errors in name section.
var tooBigNameSection = {
    name: userDefinedId,
    body: [...string(nameName), ...varU32(Math.pow(2, 31))] // declare 2**31 functions.
};
wasmEval(moduleWithSections([tooBigNameSection]));

// Skip custom sections before any expected section
var customDefSec = customSection("wee", 42, 13);
var declSec = declSection([0]);
var bodySec = bodySection([v2vBody]);
var nameSec = nameSection(null, [{name:'hi'}]);
wasmEval(moduleWithSections([customDefSec, v2vSigSection, declSec, bodySec]));
wasmEval(moduleWithSections([v2vSigSection, customDefSec, declSec, bodySec]));
wasmEval(moduleWithSections([v2vSigSection, declSec, customDefSec, bodySec]));
wasmEval(moduleWithSections([v2vSigSection, declSec, bodySec, customDefSec]));
wasmEval(moduleWithSections([customDefSec, customDefSec, v2vSigSection, declSec, bodySec]));
wasmEval(moduleWithSections([customDefSec, customDefSec, v2vSigSection, customDefSec, declSec, customDefSec, bodySec]));

// custom sections reflection:
function checkCustomSection(buf, val) {
    assertEq(buf instanceof ArrayBuffer, true);
    assertEq(buf.byteLength, 1);
    assertEq(new Uint8Array(buf)[0], val);
}
var custom1 = customSection("one", 1);
var custom2 = customSection("one", 2);
var custom3 = customSection("two", 3);
var custom4 = customSection("three", 4);
var custom5 = customSection("three", 5);
var custom6 = customSection("three", 6);
var m = new Module(moduleWithSections([custom1, v2vSigSection, custom2, declSec, custom3, bodySec, custom4, nameSec, custom5, custom6]));
var arr = Module.customSections(m, "one");
assertEq(arr.length, 2);
checkCustomSection(arr[0], 1);
checkCustomSection(arr[1], 2);
var arr = Module.customSections(m, "two");
assertEq(arr.length, 1);
checkCustomSection(arr[0], 3);
var arr = Module.customSections(m, "three");
assertEq(arr.length, 3);
checkCustomSection(arr[0], 4);
checkCustomSection(arr[1], 5);
checkCustomSection(arr[2], 6);
var arr = Module.customSections(m, "name");
assertEq(arr.length, 1);
assertEq(arr[0].byteLength, nameSec.body.length - 5 /* 4name */);

// Diagnose nonstandard block signature types.
for (var bad of [0xff, 0, 1, 0x3f])
    assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), bodySection([funcBody({locals:[], body:[BlockCode, bad, EndCode]})])])), CompileError, /invalid inline block type/);

// Ensure all invalid opcodes rejected
for (let i = FirstInvalidOpcode; i <= LastInvalidOpcode; i++) {
    let binary = moduleWithSections([v2vSigSection, declSection([0]), bodySection([funcBody({locals:[], body:[i]})])]);
    assertErrorMessage(() => wasmEval(binary), CompileError, /unrecognized opcode/);
    assertEq(WebAssembly.validate(binary), false);
}

// Prefixed opcodes
for (let prefix of [AtomicPrefix, MozPrefix]) {
    for (let i = 0; i <= 255; i++) {
	let binary = moduleWithSections([v2vSigSection, declSection([0]), bodySection([funcBody({locals:[], body:[prefix, i]})])]);
	assertErrorMessage(() => wasmEval(binary), CompileError, /unrecognized opcode/);
	assertEq(WebAssembly.validate(binary), false);
    }

    // Prefix without a subsequent opcode
    let binary = moduleWithSections([v2vSigSection, declSection([0]), bodySection([funcBody({locals:[], body:[prefix]})])]);
    assertErrorMessage(() => wasmEval(binary), CompileError, /unrecognized opcode/);
    assertEq(WebAssembly.validate(binary), false);
}

// Checking stack trace.
function runStackTraceTest(moduleName, funcNames, expectedName) {
    var sections = [
        sigSection([v2vSig]),
        importSection([{sigIndex:0, module:"env", func:"callback"}]),
        declSection([0]),
        exportSection([{funcIndex:1, name: "run"}]),
        bodySection([funcBody({locals: [], body: [CallCode, varU32(0)]})]),
        customSection("whoa"),
        customSection("wee", 42),
    ];
    if (moduleName || funcNames)
        sections.push(nameSection(moduleName, funcNames));
    sections.push(customSection("yay", 13));

    var result = "";
    var callback = () => {
        var prevFrameEntry = new Error().stack.split('\n')[1];
        result = prevFrameEntry.split('@')[0];
    };
    wasmEval(moduleWithSections(sections), {"env": { callback }}).run();
    assertEq(result, expectedName);
};

runStackTraceTest(null, null, 'wasm-function[1]');
runStackTraceTest(null, [{name:'blah'}, {name:'test'}], 'test');
runStackTraceTest(null, [{name:'test', index:1}], 'test');
runStackTraceTest(null, [{name:'blah'}, {name:'test', locals: [{name: 'var1'}, {name: 'var2'}]}], 'test');
runStackTraceTest(null, [{name:'blah'}, {name:'test', locals: [{name: 'var1'}, {name: 'var2'}]}], 'test');
runStackTraceTest(null, [{name:'blah'}, {name:'test1'}], 'test1');
runStackTraceTest(null, [{name:'blah'}, {name:'test☃'}], 'test☃');
runStackTraceTest(null, [{name:'blah'}, {name:'te\xE0\xFF'}], 'te\xE0\xFF');
runStackTraceTest(null, [{name:'blah'}], 'wasm-function[1]');
runStackTraceTest(null, [], 'wasm-function[1]');
runStackTraceTest("", [{name:'blah'}, {name:'test'}], 'test');
runStackTraceTest("a", [{name:'blah'}, {name:'test'}], 'test');
// Notice that invalid names section content shall not fail the parsing
runStackTraceTest(null, [{name:'blah'}, {name:'test', index: 2}], 'wasm-function[1]'); // invalid index
runStackTraceTest(null, [{name:'blah'}, {name:'test', index: 100000}], 'wasm-function[1]'); // invalid index
runStackTraceTest(null, [{name:'blah'}, {name:'test', nameLen: 100}], 'wasm-function[1]'); // invalid name size
runStackTraceTest(null, [{name:'blah'}, {name:''}], 'wasm-function[1]'); // empty name
