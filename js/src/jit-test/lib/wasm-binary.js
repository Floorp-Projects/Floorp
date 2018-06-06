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

