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
const dataCountId      = 12;
const eventId          = 13;

// User-defined section names
const nameName         = "name";

// Name section name types
const nameTypeModule    = 0;
const nameTypeFunction  = 1;
const nameTypeLocal     = 2;
const nameTypeEvent     = 3;

// Type codes
const I32Code          = 0x7f;
const I64Code          = 0x7e;
const F32Code          = 0x7d;
const F64Code          = 0x7c;
const V128Code         = 0x7b;
const AnyFuncCode      = 0x70;
const ExternRefCode    = 0x6f;
const EqRefCode        = 0x6d;
const OptRefCode       = 0x6c;
const FuncCode         = 0x60;
const VoidCode         = 0x40;

// Opcodes
const UnreachableCode  = 0x00
const BlockCode        = 0x02;
const TryCode          = 0x06;
const CatchCode        = 0x07;
const ThrowCode        = 0x08;
const EndCode          = 0x0b;
const ReturnCode       = 0x0f;
const CallCode         = 0x10;
const CallIndirectCode = 0x11;
const DropCode         = 0x1a;
const SelectCode       = 0x1b;
const LocalGetCode     = 0x20;
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
const RefNullCode      = 0xd0;
const RefIsNullCode    = 0xd1;
const RefFuncCode      = 0xd2;

// SIMD opcodes
const V128LoadCode = 0x00;
const V128StoreCode = 0x0b;

// Experimental SIMD opcodes as of August, 2020.
const I32x4DotSI16x8Code = 0xba;
const F32x4CeilCode = 0xd8;
const F32x4FloorCode = 0xd9;
const F32x4TruncCode = 0xda;
const F32x4NearestCode = 0xdb;
const F64x2CeilCode = 0xdc;
const F64x2FloorCode = 0xdd;
const F64x2TruncCode = 0xde;
const F64x2NearestCode = 0xdf;
const F32x4PMinCode = 0xea;
const F32x4PMaxCode = 0xeb;
const F64x2PMinCode = 0xf6;
const F64x2PMaxCode = 0xf7;
const V128Load32ZeroCode = 0xfc;
const V128Load64ZeroCode = 0xfd;

// SIMD wormhole opcodes.
const WORMHOLE_SELFTEST = 0;
const WORMHOLE_PMADDUBSW = 1;
const WORMHOLE_PMADDWD = 2;

const FirstInvalidOpcode = 0xc5;
const LastInvalidOpcode = 0xfa;
const GcPrefix = 0xfb;
const MiscPrefix = 0xfc;
const SimdPrefix = 0xfd;
const ThreadPrefix = 0xfe;
const MozPrefix = 0xff;

// See WasmConstants.h for documentation.
// Limit this to a group of 8 per line.

const definedOpcodes =
    [0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
     ...(wasmExceptionsEnabled() ? [0x06, 0x07, 0x08] : []),
     0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
     0x10, 0x11,
     0x1a, 0x1b, 0x1c,
     0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
     0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
     0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
     0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
     0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
     0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
     0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
     0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
     0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
     0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
     0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
     0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
     0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
     0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
     0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
     0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
     0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
     0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
     0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
     0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
     0xc0, 0xc1, 0xc2, 0xc3, 0xc4,
     0xd0, 0xd1, 0xd2,
     0xf0,
     0xfc, 0xfd, 0xfe, 0xff ];

const undefinedOpcodes = (function () {
    let a = [];
    let j = 0;
    let i = 0;
    while (i < 256) {
        while (definedOpcodes[j] > i)
            a.push(i++);
        assertEq(definedOpcodes[j], i);
        i++;
        j++;
    }
    assertEq(definedOpcodes.length + a.length, 256);
    return a;
})();

// Secondary opcode bytes for misc prefix
const MemoryInitCode = 0x08;    // Pending
const DataDropCode = 0x09;      // Pending
const MemoryCopyCode = 0x0a;    // Pending
const MemoryFillCode = 0x0b;    // Pending
const TableInitCode = 0x0c;     // Pending
const ElemDropCode = 0x0d;      // Pending
const TableCopyCode = 0x0e;     // Pending

const StructNew = 0x00;         // UNOFFICIAL
const StructGet = 0x03;         // UNOFFICIAL
const StructSet = 0x06;         // UNOFFICIAL

// DefinitionKind
const FunctionCode     = 0x00;
const TableCode        = 0x01;
const MemoryCode       = 0x02;
const GlobalCode       = 0x03;
const EventCode        = 0x04;

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

function funcBody(func, withEndCode=true) {
    var body = varU32(func.locals.length);
    for (let local of func.locals)
        body.push(...varU32(local));
    body = body.concat(...func.body);
    if (withEndCode)
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
        if (exp.hasOwnProperty("funcIndex")) {
            body.push(...varU32(FunctionCode));
            body.push(...varU32(exp.funcIndex));
        } else if (exp.hasOwnProperty("memIndex")) {
            body.push(...varU32(MemoryCode));
            body.push(...varU32(exp.memIndex));
        } else if (exp.hasOwnProperty("eventIndex")) {
            body.push(...varU32(EventCode));
            body.push(...varU32(exp.eventIndex));
        } else {
            throw "Bad export " + exp;
        }
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

function eventSection(events) {
    var body = [];
    body.push(...varU32(events.length));
    for (let event of events) {
        body.push(...varU32(0)); // exception attribute
        body.push(...varU32(event.type));
    }
    return { name: eventId, body };
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

function dataCountSection(count) {
    var body = [];
    body.push(...varU32(count));
    return { name: dataCountId, body };
}

function globalSection(globalArray) {
    var body = [];
    body.push(...varU32(globalArray.length));
    for (let globalObj of globalArray) {
        // Value type
        body.push(...varU32(globalObj.valType));
        // Flags
        body.push(globalObj.flags & 255);
        // Initializer expression
        body.push(...globalObj.initExpr);
    }
    return { name: globalId, body };
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

// For now, the encoding spec is here:
// https://github.com/WebAssembly/bulk-memory-operations/issues/98#issuecomment-507330729

const LegacyActiveExternVal = 0;
const PassiveExternVal = 1;
const ActiveExternVal = 2;
const DeclaredExternVal = 3;
const LegacyActiveElemExpr = 4;
const PassiveElemExpr = 5;
const ActiveElemExpr = 6;
const DeclaredElemExpr = 7;

function generalElemSection(elemObjs) {
    let body = [];
    body.push(...varU32(elemObjs.length));
    for (let elemObj of elemObjs) {
        body.push(elemObj.flag);
        if ((elemObj.flag & 3) == 2)
            body.push(...varU32(elemObj.table));
        // TODO: This is not very flexible
        if ((elemObj.flag & 1) == 0) {
            body.push(...varU32(I32ConstCode));
            body.push(...varS32(elemObj.offset));
            body.push(...varU32(EndCode));
        }
        if (elemObj.flag & 4) {
            if (elemObj.flag & 3)
                body.push(elemObj.typeCode & 255);
            // Each element is an array of bytes
            body.push(...varU32(elemObj.elems.length));
            for (let elemBytes of elemObj.elems)
                body.push(...elemBytes);
        } else {
            if (elemObj.flag & 3)
                body.push(elemObj.externKind & 255);
            // Each element is a putative function index
            body.push(...varU32(elemObj.elems.length));
            for (let elem of elemObj.elems)
                body.push(...varU32(elem));
        }
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
