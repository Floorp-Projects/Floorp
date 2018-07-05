// MagicNumber = 0x6d736100;
const magic0 = 0x00;  // '\0'
const magic1 = 0x61;  // 'a'
const magic2 = 0x73;  // 's'
const magic3 = 0x6d;  // 'm'

// EncodingVersion
const encodingVersion = 0x1;
const ver0 = (encodingVersion >>>  0) & 0xff;
const ver1 = (encodingVersion >>>  8) & 0xff;
const ver2 = (encodingVersion >>> 16) & 0xff;
const ver3 = (encodingVersion >>> 24) & 0xff;

// Section opcodes
const userDefinedId    = 0;
const typeId           = 1;
const importId         = 2;
const functionId       = 3;
const tableId          = 4;
const memoryId         = 5;
const globalId         = 6;
const exportId         = 7;
const startId          = 8;
const elemId           = 9;
const codeId           = 10;
const dataId           = 11;

// User-defined section names
const nameName         = "name";

// Name section name types
const nameTypeModule   = 0;
const nameTypeFunction = 1;
const nameTypeLocal    = 2;

// Type codes
const I32Code          = 0x7f;
const I64Code          = 0x7e;
const F32Code          = 0x7d;
const F64Code          = 0x7c;
const AnyFuncCode      = 0x70;
const FuncCode         = 0x60;
const VoidCode         = 0x40;

// Opcodes
const UnreachableCode  = 0x00
const BlockCode        = 0x02;
const EndCode          = 0x0b;
const CallCode         = 0x10;
const CallIndirectCode = 0x11;
const I32Load          = 0x28;
const I64Load          = 0x29;
const F32Load          = 0x2a;
const F64Load          = 0x2b;
const I32Load8S        = 0x2c;
const I32Load8U        = 0x2d;
const I32Load16S       = 0x2e;
const I32Load16U       = 0x2f;
const I64Load8S        = 0x30;
const I64Load8U        = 0x31;
const I64Load16S       = 0x32;
const I64Load16U       = 0x33;
const I64Load32S       = 0x34;
const I64Load32U       = 0x35;
const I32Store         = 0x36;
const I64Store         = 0x37;
const F32Store         = 0x38;
const F64Store         = 0x39;
const I32Store8        = 0x3a;
const I32Store16       = 0x3b;
const I64Store8        = 0x3c;
const I64Store16       = 0x3d;
const I64Store32       = 0x3e;
const GrowMemoryCode   = 0x40;
const I32ConstCode     = 0x41;
const I64ConstCode     = 0x42;
const F32ConstCode     = 0x43;
const F64ConstCode     = 0x44;
const I32AddCode       = 0x6a;
const I32DivSCode      = 0x6d;
const I32DivUCode      = 0x6e;
const I32RemSCode      = 0x6f;
const I32RemUCode      = 0x70;
const I32TruncSF32Code = 0xa8;
const I32TruncUF32Code = 0xa9;
const I32TruncSF64Code = 0xaa;
const I32TruncUF64Code = 0xab;
const I64TruncSF32Code = 0xae;
const I64TruncUF32Code = 0xaf;
const I64TruncSF64Code = 0xb0;
const I64TruncUF64Code = 0xb1;
const I64DivSCode      = 0x7f;
const I64DivUCode      = 0x80;
const I64RemSCode      = 0x81;
const I64RemUCode      = 0x82;

const FirstInvalidOpcode = 0xc5;
const LastInvalidOpcode = 0xfb;
const MiscPrefix = 0xfc;
const SimdPrefix = 0xfd;
const ThreadPrefix = 0xfe;
const MozPrefix = 0xff;

// DefinitionKind
const FunctionCode     = 0x00;
const TableCode        = 0x01;
const MemoryCode       = 0x02;
const GlobalCode       = 0x03;

// ResizableFlags
const HasMaximumFlag   = 0x1;

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

function moduleHeaderThen(...rest) {
    return [magic0, magic1, magic2, magic3, ver0, ver1, ver2, ver3, ...rest];
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

function moduleNameSubsection(moduleName) {
    var body = [];
    body.push(...varU32(nameTypeModule));

    var subsection = encodedString(moduleName);
    body.push(...varU32(subsection.length));
    body.push(...subsection);

    return body;
}

function funcNameSubsection(funcNames) {
    var body = [];
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
    return body;
}

function nameSection(subsections) {
    var body = [];
    body.push(...string(nameName));

    for (let ss of subsections)
        body.push(...ss);

    return { name: userDefinedId, body };
}

function customSection(name, ...body) {
    return { name: userDefinedId, body: [...string(name), ...body] };
}

function tableSection0() {
    var body = [];
    body.push(...varU32(0));           // number of tables
    return { name: tableId, body };
}

function memorySection0() {
    var body = [];
    body.push(...varU32(0));           // number of memories
    return { name: memoryId, body };
}
