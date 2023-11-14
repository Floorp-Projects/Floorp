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
const tagId            = 13;

// User-defined section names
const nameName         = "name";

// Name section name types
const nameTypeModule    = 0;
const nameTypeFunction  = 1;
const nameTypeLocal     = 2;
const nameTypeTag       = 3;

// Type codes
const I32Code          = 0x7f;
const I64Code          = 0x7e;
const F32Code          = 0x7d;
const F64Code          = 0x7c;
const V128Code         = 0x7b;
const AnyFuncCode      = 0x70;
const ExternRefCode    = 0x6f;
const EqRefCode        = 0x6d;
const OptRefCode       = 0x63; // (ref null $t), needs heap type immediate
const RefCode          = 0x64; // (ref $t), needs heap type immediate
const FuncCode         = 0x60;
const StructCode       = 0x5f;
const ArrayCode        = 0x5e;
const VoidCode         = 0x40;
const BadType          = 0x79; // reserved for testing

// Opcodes
const UnreachableCode  = 0x00
const BlockCode        = 0x02;
const TryCode          = 0x06;
const CatchCode        = 0x07;
const ThrowCode        = 0x08;
const RethrowCode      = 0x09;
const EndCode          = 0x0b;
const ReturnCode       = 0x0f;
const CallCode         = 0x10;
const CallIndirectCode = 0x11;
const ReturnCallCode   = 0x12;
const ReturnCallIndirectCode = 0x13;
const ReturnCallRefCode      = 0x15;
const DelegateCode     = 0x18;
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

// Relaxed SIMD opcodes.
const I8x16RelaxedSwizzleCode = 0x100;
const I32x4RelaxedTruncSSatF32x4Code = 0x101;
const I32x4RelaxedTruncUSatF32x4Code = 0x102;
const I32x4RelaxedTruncSatF64x2SZeroCode = 0x103;
const I32x4RelaxedTruncSatF64x2UZeroCode = 0x104;
const F32x4RelaxedMaddCode = 0x105;
const F32x4RelaxedNmaddCode = 0x106;
const F64x2RelaxedMaddCode = 0x107;
const F64x2RelaxedNmaddCode = 0x108;
const I8x16RelaxedLaneSelectCode = 0x109;
const I16x8RelaxedLaneSelectCode = 0x10a;
const I32x4RelaxedLaneSelectCode = 0x10b;
const I64x2RelaxedLaneSelectCode = 0x10c;
const F32x4RelaxedMinCode = 0x10d;
const F32x4RelaxedMaxCode = 0x10e;
const F64x2RelaxedMinCode = 0x10f;
const F64x2RelaxedMaxCode = 0x110;
const I16x8RelaxedQ15MulrSCode = 0x111;
const I16x8DotI8x16I7x16SCode = 0x112;
const I32x4DotI8x16I7x16AddSCode = 0x113;

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
     ...(wasmExceptionsEnabled() ? [0x06, 0x07, 0x08, 0x09] : []),
     0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
     0x10, 0x11,
     ...(wasmTailCallsEnabled() ? [0x12, 0x13] : []),
     ...(wasmFunctionReferencesEnabled() ? [0x14] : []),
     ...(wasmTailCallsEnabled() &&
         wasmFunctionReferencesEnabled() ? [0x15] : []),
     ...(wasmExceptionsEnabled() ? [0x18, 0x19] : []),
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
     0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
     0xf0,
     0xfb, 0xfc, 0xfd, 0xfe, 0xff ];

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
const TagCode          = 0x04;

// ResizableFlags
const HasMaximumFlag   = 0x1;

function toU8(array) {
    for (let b of array)
        assertEq(b < 256, true);
    return Uint8Array.from(array);
}

function varU32(u32) {
    assertEq(u32 >= 0, true, `varU32 input must be number between 0 and 2^32-1, got ${u32}`);
    assertEq(u32 < Math.pow(2,32), true, `varU32 input must be number between 0 and 2^32-1, got ${u32}`);
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
    assertEq(s32 >= -Math.pow(2,31), true, `varS32 input must be number between -2^31 and 2^31-1, got ${s32}`);
    assertEq(s32 < Math.pow(2,31), true, `varS32 input must be number between -2^31 and 2^31-1, got ${s32}`);
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

/**
 * Creates a type section for a module. Example:
 *
 *     typeSection([
 *         // (type (func (param i32 i64)))
 *         { kind: FuncCode, args: [I32Code, I64Code], ret: [] },
 *         // (type (func (result (ref 123))))
 *         { kind: FuncCode, args: [], ret: [[RefCode, ...varS32(123)]] },
 *
 *         // GC types are supported:
 *         { kind: StructCode, fields: [I32Code, { mut: true, type: [RefCode, ...varS32(123)] }] },
 *         { kind: ArrayCode, elem: { mut: true, type: I32Code } }] },
 *         { kind: ArrayCode, elem: { mut: true, type: [RefCode, ...varS32(123)] } }] },
 *
 *         // Recursion groups can be created with the recGroup function
 *         recGroup([
 *             { kind: StructCode, fields: [I32Code, I64Code] },
 *             { kind: StructCode, sub: 5, fields: [I32Code, I64Code, I32Code] },
 *         ]),
 *     ])
 *
 * ## Full documentation
 *
 * This function takes an array of type objects in one of the following formats:
 *
 *     { kind: FuncCode, args: <ResultType>, ret: <ResultType> }
 *     { kind: StructCode, fields: [<FieldType>] }
 *     { kind: ArrayCode, elem: <FieldType> }
 *
 * Each type object can also have the following optional fields:
 *
 *   - `sub: <number>`: Makes the type a subtype of the given type index.
 *     By default it will not have any parent types.
 *   - `final: <boolean>`: Controls whether the type is final. Default `true`.
 *
 * And finally, types can be placed in a recursion group by wrapping them
 * with the `recGroup` function.
 *
 * ### ResultType
 *
 * A result type is a vector of value types. You provide this as an array
 * where each entry is the bytes for the type. For example, for a function
 * with `(return i32 (ref 123))`, you might provide:
 *
 *     [[I32Code], [RefCode, ...varS32(123)]]
 *
 * If a value type is only a single byte, you can pass it directly instead of
 * passing an array:
 *
 *     [I32Code, [RefCode, ...varS32(123)]]
 *
 * If there is only a single value type, you can omit the outer array too:
 *
 *     I32Code // same as [I32Code], same as [[I32Code]]
 *
 * And finally, `VoidCode` is a special case that results in an empty vector.
 *
 *     VoidCode // same as []
 *
 * Note that if you want to encode a single type, but that type has multiple
 * bytes, you will need to keep the outermost array.
 *
 *     [I32Code, I64Code]        // sugar for [[I32Code], [I64Code]], so two types
 *     [RefCode, ...varS32(123)] // will be interpreted as [[RefCode], [123]],
 *                               // i.e. two types - not what you want
 *
 * ### FieldType
 *
 * A field type is used for struct and array values, and is a value type plus
 * mutability info. The general form looks like:
 *
 *     { mut: <boolean>, type: <bytes> }
 *
 * For example, `(mut i32)` would look like:
 *
 *     { mut: true, type: [I32Code] }
 *
 * If the type is a single byte, you can omit the array:
 *
 *     { mut: true, type: I32Code }
 *
 * And if you wish for the field to be immutable, you can provide the type only:
 *
 *     I32Code // same as { mut: false, type: I32Code }
 *
 */
function typeSection(types) {
    var body = [];
    body.push(...varU32(types.length)); // technically a count of recursion groups
    for (const type of types) {
        if (type.isRecursionGroup) {
            body.push(0x4f);
            body.push(...varU32(type.types.length));
            for (const t of type.types) {
                body.push(..._encodeType(t));
            }
        } else {
            body.push(..._encodeType(type));
        }
    }
    return { name: typeId, body };
}

function recGroup(types) {
    return { isRecursionGroup: true, types };
}

/**
 * Returns a "normalized" version of all the ResultType stuff from `typeSection`,
 * i.e. an array of array of bytes for each value type.
 */
function _resultType(input) {
    if (input === VoidCode) {
        return [];
    }
    if (typeof input === "number") {
        input = [input];
    }
    input = input.map(valType => Array.isArray(valType) ? valType : [valType]);
    return input;
}

/**
 * Returns a "normalized" version of FieldType from `typeSection`, i.e. an object
 * of the form `{ mut: <boolean>, type: <bytes> }`.
 */
function _fieldType(input) {
    if (typeof input !== "object" || Array.isArray(input)) {
        input = { mut: false, type: input };
    }
    if (!Array.isArray(input.type)) {
        input.type = [input.type];
    }
    return input;
}

/**
 * Encodes a type object from `typeSection`. This basically corresponds to `subtypeDef`
 * in the GC spec doc.
 */
function _encodeType(typeObj) {
    const typeBytes = [];
    // Types are now final by default.
    const final = typeObj.final ?? true;
    if (typeObj.sub !== undefined) {
        typeBytes.push(final ? 0x4e : 0x50);
        typeBytes.push(...varU32(1), ...varU32(typeObj.sub));
    }
    else if (final == false) {
        // This type is extensible even if no supertype is defined.
        typeBytes.push(0x50);
        typeBytes.push(0x00);
    }
    typeBytes.push(typeObj.kind);
    switch (typeObj.kind) {
    case FuncCode: {
        const args = _resultType(typeObj.args);
        const ret = _resultType(typeObj.ret);
        typeBytes.push(...varU32(args.length));
        for (const t of args) {
            typeBytes.push(...t);
        }
        typeBytes.push(...varU32(ret.length));
        for (const t of ret) {
            typeBytes.push(...t);
        }
    } break;
    case StructCode: {
        // fields
        typeBytes.push(...varU32(typeObj.fields.length));
        for (const f of typeObj.fields) {
            typeBytes.push(..._encodeFieldType(f));
        }
    } break;
    case ArrayCode: {
        // elem
        typeBytes.push(..._encodeFieldType(typeObj.elem));
    } break;
    default:
        throw new Error(`unknown type kind ${typeObj.kind} in type section`);
    }
    return typeBytes;
}

function _encodeFieldType(fieldTypeObj) {
    fieldTypeObj = _fieldType(fieldTypeObj);
    return [...fieldTypeObj.type, fieldTypeObj.mut ? 0x01 : 0x00];
}

/**
 * A convenience function to create a type section containing only function
 * types. This is basically sugar for `typeSection`, although you do not have
 * to provide `kind: FuncCode` on each definition as you would there.
 *
 * Example:
 *
 *     sigSection([
 *         // (type (func (param i32 i64)))
 *         { args: [I32Code, I64Code], ret: [] },
 *         // (type (func (result (ref 123))))
 *         { args: [], ret: [[RefCode, ...varS32(123)]] },
 *     ])
 *
 */
function sigSection(sigs) {
    return typeSection(sigs.map(sig => ({ kind: FuncCode, ...sig })));
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
        } else if (exp.hasOwnProperty("tagIndex")) {
            body.push(...varU32(TagCode));
            body.push(...varU32(exp.tagIndex));
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

function tagSection(tags) {
    var body = [];
    body.push(...varU32(tags.length));
    for (let tag of tags) {
        body.push(...varU32(0)); // exception attribute
        body.push(...varU32(tag.type));
    }
    return { name: tagId, body };
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
