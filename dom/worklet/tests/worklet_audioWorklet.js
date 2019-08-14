class DummyProcessWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process() {
    // Do nothing, output silence
  }
}

// We need to pass a valid AudioWorkletProcessor here, otherwise, it will fail,
// and the console.log won't be executed
registerProcessor("sure!", DummyProcessWorkletProcessor);
console.log(
  globalThis instanceof AudioWorkletGlobalScope ? "So far so good" : "error"
);
