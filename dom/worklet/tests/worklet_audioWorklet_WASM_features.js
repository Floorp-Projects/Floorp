class WasmProcessWorkletProcessor extends AudioWorkletProcessor {
  constructor(...args) {
    super(...args);
    this.port.postMessage(testModules());
  }

  process(inputs, outputs, parameters) {
    // Do nothing, output silence
    return true;
  }
}

function testModule(binary) {
  try {
    let wasmModule = new WebAssembly.Module(binary);
  } catch (error) {
    if (error instanceof WebAssembly.CompileError) {
      return error.message;
    }
    return "unknown error";
  }
  return true;
}

// TODO: test more features
function testModules() {
  /*
    js -e '
      t = wasmTextToBinary(`
        (module
          (tag)
        )
      `);
      print(t)
    '
  */
  // eslint-disable-next-line
  const exceptionHandlingCode = new Uint8Array([
    0, 97, 115, 109, 1, 0, 0, 0, 1, 4, 1, 96, 0, 0, 13, 3, 1, 0, 0,
  ]);
  return testModule(exceptionHandlingCode);
}

registerProcessor("wasm", WasmProcessWorkletProcessor);
