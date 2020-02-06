class WasmProcessWorkletProcessor extends AudioWorkletProcessor {
  constructor(...args) {
    super(...args);
    this.port.onmessage = e => {
      WebAssembly.compile(e.data).then(
        m => {
          this.port.postMessage(m);
        },
        () => {
          this.port.postMessage("error");
        }
      );
    };
  }

  process(inputs, outputs, parameters) {
    // Do nothing, output silence
    return true;
  }
}

registerProcessor("promise", WasmProcessWorkletProcessor);
