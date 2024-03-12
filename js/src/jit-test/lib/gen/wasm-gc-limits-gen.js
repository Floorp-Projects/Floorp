// Generates large .wasm files for use in ../limits.js.
// Make sure you are running this script from a release build or you will be sad.

loadRelativeToScript("../wasm-binary.js");

function moduleNRecGroupNTypes(numRecs, numTypes) {
  let types = [];
  for (let i = 0; i < numTypes; i++) {
    types.push({ kind: FuncCode, args: [], ret: [] });
  }
  let recs = [];
  for (let i = 0; i < numRecs; i++) {
    recs.push(recGroup(types));
  }
  return new Uint8Array(compressLZ4(new Uint8Array(moduleWithSections([typeSection(recs)])).buffer));
}

os.file.writeTypedArrayToFile("wasm-gc-limits-r1M-t1.wasm", moduleNRecGroupNTypes(1_000_000, 1));
os.file.writeTypedArrayToFile("wasm-gc-limits-r1M1-t1.wasm", moduleNRecGroupNTypes(1_000_001, 1));
os.file.writeTypedArrayToFile("wasm-gc-limits-r1-t1M.wasm", moduleNRecGroupNTypes(1, 1_000_000));
os.file.writeTypedArrayToFile("wasm-gc-limits-r1-t1M1.wasm", moduleNRecGroupNTypes(1, 1_000_001));
os.file.writeTypedArrayToFile("wasm-gc-limits-r2-t500K.wasm", moduleNRecGroupNTypes(2, 500_000));
os.file.writeTypedArrayToFile("wasm-gc-limits-r2-t500K1.wasm", moduleNRecGroupNTypes(2, 500_001));

function moduleLargeStruct(size) {
  let structInitializer = [];
  for (let i = 0; i < size; i++) {
    structInitializer.push(I64ConstCode);
    structInitializer.push(...varU32(0));
  }
  return new Uint8Array(compressLZ4(new Uint8Array(moduleWithSections([
    typeSection([
      {
        kind: StructCode,
        fields: Array(size).fill(I64Code)
      },
      {
        kind: FuncCode,
        args: [],
        ret: [AnyRefCode]
      }
    ]),
    declSection([1, 1]),
    exportSection([
      {name: "makeLargeStructDefault", funcIndex: 0},
      {name: "makeLargeStruct", funcIndex: 1}
    ]),
    bodySection([
      funcBody({
        locals: [],
        body: [
          GcPrefix,
          StructNewDefault,
          ...varU32(0)
        ],
      }),
      funcBody({
        locals: [],
        body: [
          ...structInitializer,
          GcPrefix,
          StructNew,
          ...varU32(0)
        ],
      }),
    ]),
  ])).buffer));
}

os.file.writeTypedArrayToFile("wasm-gc-limits-s10K.wasm", moduleLargeStruct(10_000));
os.file.writeTypedArrayToFile("wasm-gc-limits-s10K1.wasm", moduleLargeStruct(10_001));
