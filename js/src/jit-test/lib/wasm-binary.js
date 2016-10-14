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

// User-defined section names
const nameName = "name";

// Type codes
const VoidCode = 0;
const I32Code = 1;
const I64Code = 2;
const F32Code = 3;
const F64Code = 4;

// Type constructors
const AnyFuncCode = 0x20;
const FunctionConstructorCode = 0x40;

// Opcodes
const UnreachableCode  = 0x00;
const BlockCode        = 0x01;
const EndCode          = 0x0f;
const I32ConstCode     = 0x10;
const I64ConstCode     = 0x11;
const F64ConstCode     = 0x12;
const F32ConstCode     = 0x13;
const CallCode         = 0x16;
const CallIndirectCode = 0x17;
const I32Load8S        = 0x20;
const I32Load8U        = 0x21;
const I32Load16S       = 0x22;
const I32Load16U       = 0x23;
const I64Load8S        = 0x24;
const I64Load8U        = 0x25;
const I64Load16S       = 0x26;
const I64Load16U       = 0x27;
const I64Load32S       = 0x28;
const I64Load32U       = 0x29;
const I32Load          = 0x2a;
const I64Load          = 0x2b;
const F32Load          = 0x2c;
const F64Load          = 0x2d;
const I32Store8        = 0x2e;
const I32Store16       = 0x2f;
const I64Store8        = 0x30;
const I64Store16       = 0x31;
const I64Store32       = 0x32;
const I32Store         = 0x33;
const I64Store         = 0x34;
const F32Store         = 0x35;
const F64Store         = 0x36;
const I32DivSCode      = 0x43;
const I32DivUCode      = 0x44;
const I32RemSCode      = 0x45;
const I32RemUCode      = 0x46;
const I32TruncSF32Code = 0x9d;
const I32TruncSF64Code = 0x9e;
const I32TruncUF32Code = 0x9f;
const I32TruncUF64Code = 0xa0;
const I64TruncSF32Code = 0xa2;
const I64TruncSF64Code = 0xa3;
const I64TruncUF32Code = 0xa4;
const I64TruncUF64Code = 0xa5;
const GrowMemoryCode   = 0x39;

// DefinitionKind
const FunctionCode = 0x00;
const TableCode    = 0x01;
const MemoryCode   = 0x02;
const GlobalCode   = 0x03;

// ResizableFlags
const DefaultFlag    = 0x1;
const HasMaximumFlag = 0x2;

