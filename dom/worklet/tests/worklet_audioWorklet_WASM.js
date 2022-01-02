class WasmProcessWorkletProcessor extends AudioWorkletProcessor {
  constructor(...args) {
    super(...args);
    this.port.onmessage = e => {
      // Let's send it back.
      this.port.postMessage(e.data);
    };
  }

  process(inputs, outputs, parameters) {
    // Do nothing, output silence
    return true;
  }
}

registerProcessor("wasm", WasmProcessWorkletProcessor);
