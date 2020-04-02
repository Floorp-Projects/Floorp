load(libdir + "wasm-binary.js");

const { extractStackFrameFunction } = WasmHelpers;

const { Module, RuntimeError, CompileError } = WebAssembly;

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
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(37))), CompileError, unknownSection);
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(37, 0))), CompileError, unknownSection);
assertErrorMessage(() => wasmEval(toU8(moduleHeaderThen(37, 1, 0))), CompileError, unknownSection);

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

const v2vSig = {args:[], ret:VoidCode};
const v2vSigSection = sigSection([v2vSig]);
const i2vSig = {args:[I32Code], ret:VoidCode};
const v2vBody = funcBody({locals:[], body:[]});

assertErrorMessage(() => wasmEval(moduleWithSections([ {name: typeId, body: U32MAX_LEB } ])), CompileError, /too many types/);
assertErrorMessage(() => wasmEval(moduleWithSections([ {name: typeId, body: [1, 0], } ])), CompileError, /expected type form/);
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
assertErrorMessage(() => wasmEval(moduleWithSections([v2vSigSection, declSection([0])])), CompileError, /expected code section/);
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

wasmEval(moduleWithSections([tableSection0()]));

wasmEval(moduleWithSections([memorySection(0)]));

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
var nameSec = nameSection([funcNameSubsection([{name:'hi'}])]);
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

// Test name/custom section warnings:
const nameWarning = /validated with warning.*'name' custom section/;
const okNameSec = nameSection([]);
assertNoWarning(() => wasmEval(moduleWithSections([v2vSigSection, declSec, bodySec, okNameSec])));
const badNameSec1 = nameSection([]);
badNameSec1.body.push(1);
assertWarning(() => wasmEval(moduleWithSections([v2vSigSection, declSec, bodySec, badNameSec1])), nameWarning);
const badNameSec2 = nameSection([funcNameSubsection([{name:'blah'}])]);
badNameSec2.body.push(100, 20, 42, 83);
assertWarning(() => wasmEval(moduleWithSections([v2vSigSection, declSec, bodySec, badNameSec2])), nameWarning);
const badNameSec3 = nameSection([funcNameSubsection([{name:'blah'}])]);
badNameSec3.body.pop();
assertWarning(() => wasmEval(moduleWithSections([v2vSigSection, declSec, bodySec, badNameSec3])), nameWarning);
assertNoWarning(() => wasmEval(moduleWithSections([nameSection([moduleNameSubsection('hi')])])));
assertWarning(() => wasmEval(moduleWithSections([nameSection([moduleNameSubsection('hi'), moduleNameSubsection('boo')])])), nameWarning);
// Unknown name subsection
assertNoWarning(() => wasmEval(moduleWithSections([nameSection([moduleNameSubsection('hi'), [4, 0]])])));
assertWarning(() => wasmEval(moduleWithSections([nameSection([moduleNameSubsection('hi'), [4, 1]])])), nameWarning);
assertNoWarning(() => wasmEval(moduleWithSections([nameSection([moduleNameSubsection('hi'), [4, 1, 42]])])));

// Provide a module name but no function names.
assertErrorMessage(() => wasmEval(moduleWithSections([
    v2vSigSection,
    declSection([0]),
    exportSection([{funcIndex: 0, name: "f"}]),
    bodySection([funcBody({locals:[], body:[UnreachableCode]})]),
    nameSection([moduleNameSubsection('hi')])])
).f(), RuntimeError, /unreachable/);

// Diagnose invalid block signature types.
for (var bad of [0xff, 1, 0x3f])
    assertErrorMessage(() => wasmEval(moduleWithSections([sigSection([v2vSig]), declSection([0]), bodySection([funcBody({locals:[], body:[BlockCode, bad, EndCode]})])])), CompileError, /invalid .*block type/);

const multiValueModule = moduleWithSections([sigSection([v2vSig]), declSection([0]), bodySection([funcBody({locals:[], body:[BlockCode, 0, EndCode]})])]);
if (wasmMultiValueEnabled()) {
    // In this test module, 0 denotes a void-to-void block type.
    assertEq(WebAssembly.validate(multiValueModule), true);
} else {
    assertErrorMessage(() => wasmEval(multiValueModule), CompileError, /invalid .*block type/);
}

// Ensure all invalid opcodes rejected
for (let op of undefinedOpcodes) {
    let binary = moduleWithSections([v2vSigSection, declSection([0]), bodySection([funcBody({locals:[], body:[op]})])]);
    assertErrorMessage(() => wasmEval(binary), CompileError, /unrecognized opcode/);
    assertEq(WebAssembly.validate(binary), false);
}

// Prefixed opcodes

function checkIllegalPrefixed(prefix, opcode) {
    let binary = moduleWithSections([v2vSigSection,
                                     declSection([0]),
                                     bodySection([funcBody({locals:[],
                                                            body:[prefix, ...varU32(opcode)]})])]);
    assertErrorMessage(() => wasmEval(binary), CompileError, /unrecognized opcode/);
    assertEq(WebAssembly.validate(binary), false);
}

// Illegal GcPrefix opcodes

let reservedGc = {};
if (wasmGcEnabled()) {
    reservedGc = {
        0x0: true,
        0x3: true,
        0x6: true,
        0x7: true
    };
}
for (let i = 0; i < 256; i++) {
    if (reservedGc.hasOwnProperty(i)) {
        continue;
    }
    checkIllegalPrefixed(GcPrefix, i);
}

// Illegal ThreadPrefix opcodes
//
// June 2017 threads draft:
//
//  0x00 .. 0x03 are wait/wake/fence ops
//  0x10 .. 0x4f are primitive atomic ops

for (let i = 0x4; i < 0x10; i++)
    checkIllegalPrefixed(ThreadPrefix, i);

for (let i = 0x4f; i < 0x100; i++)
    checkIllegalPrefixed(ThreadPrefix, i);

// Illegal Misc opcodes

var reservedMisc =
    { // Saturating conversions (standardized)
      0x00: true, 0x01: true, 0x02: true, 0x03: true, 0x04: true, 0x05: true, 0x06: true, 0x07: true,
      // Bulk memory (proposed)
      0x08: true, 0x09: true, 0x0a: true, 0x0b: true, 0x0c: true, 0x0d: true, 0x0e: true,
      // Table (proposed)
      0x0f: true, 0x10: true, 0x11: true, 0x12: true,
      // Structure operations (experimental, internal)
      0x50: true, 0x51: true, 0x52: true, 0x53: true };

for (let i = 0; i < 256; i++) {
    if (reservedMisc.hasOwnProperty(i))
        continue;
    checkIllegalPrefixed(MiscPrefix, i);
}

// Illegal SIMD opcodes (all of them, for now)
for (let i = 0; i < 256; i++)
    checkIllegalPrefixed(SimdPrefix, i);

// Illegal MozPrefix opcodes (all of them)
for (let i = 0; i < 256; i++)
    checkIllegalPrefixed(MozPrefix, i);

for (let prefix of [ThreadPrefix, MiscPrefix, SimdPrefix, MozPrefix]) {
    // Prefix without a subsequent opcode.  We must ask funcBody not to add an
    // End code after the prefix, so the body really is just the prefix byte.
    let binary = moduleWithSections([v2vSigSection, declSection([0]), bodySection([funcBody({locals:[], body:[prefix]}, /*withEndCode=*/false)])]);
    assertErrorMessage(() => wasmEval(binary), CompileError, /unable to read opcode/);
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
    if (moduleName || funcNames) {
        var subsections = [];
        if (moduleName)
            subsections.push(moduleNameSubsection(moduleName));
        if (funcNames)
            subsections.push(funcNameSubsection(funcNames));
        sections.push(nameSection(subsections));
    }
    sections.push(customSection("yay", 13));

    var result = "";
    var callback = () => {
        result = extractStackFrameFunction(new Error().stack.split('\n')[1]);
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
runStackTraceTest("a", [{name:'blah'}, {name:'test'}], 'a.test');
// Notice that invalid names section content shall not fail the parsing
runStackTraceTest(null, [{name:'blah'}, {name:'test', index: 2}], 'wasm-function[1]'); // invalid index
runStackTraceTest(null, [{name:'blah'}, {name:'test', index: 100000}], 'wasm-function[1]'); // invalid index
runStackTraceTest(null, [{name:'blah'}, {name:'test', nameLen: 100}], 'wasm-function[1]'); // invalid name size
runStackTraceTest(null, [{name:'blah'}, {name:''}], 'wasm-function[1]'); // empty name

// Enable and disable Gecko profiling mode, to ensure all live instances
// names won't make us crash.
enableGeckoProfiling();
disableGeckoProfiling();

function testValidNameSectionWithProfiling() {
    enableGeckoProfiling();
    wasmEval(moduleWithSections([v2vSigSection, declSec, bodySec, nameSec]));
    disableGeckoProfiling();
}
testValidNameSectionWithProfiling();
