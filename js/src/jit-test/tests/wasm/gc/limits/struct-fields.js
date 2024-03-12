// |jit-test| --setpref=wasm_gc; test-also=--wasm-compiler=optimizing; test-also=--wasm-compiler=baseline; include:wasm.js;

loadRelativeToScript("load-mod.js");

// Limit of 10_000 struct fields
wasmFailValidateBinary(loadMod("wasm-gc-limits-s10K1.wasm"), /too many/);
{
  let {makeLargeStructDefault, makeLargeStruct} = wasmEvalBinary(loadMod("wasm-gc-limits-s10K.wasm")).exports;
  let largeStructDefault = makeLargeStructDefault();
  let largeStruct = makeLargeStruct();
}
