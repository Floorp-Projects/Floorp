// Define several classes.
// Only the last ones are valid.
class EmptyWorkletProcessor extends AudioWorkletProcessor {
}

class NoProcessWorkletProcessor extends AudioWorkletProcessor {
  constructor() { super(); }
}

class BadDescriptorsWorkletProcessor extends AudioWorkletProcessor {
  constructor() { super(); }

  process() {
    // Do nothing, output silence
  }

  static get parameterDescriptors() {
    return "A string";
  }
}

class GoodDescriptorsWorkletProcessor extends AudioWorkletProcessor {
  constructor() { super(); }

  process() {
    // Do nothing, output silence
  }

  static get parameterDescriptors() {
    return [{
      name: 'myParam', defaultValue: 0.707
    }];
  }
}

class DummyProcessWorkletProcessor extends AudioWorkletProcessor {
  constructor() { super(); }

  process() {
    // Do nothing, output silence
  }
}

// Test not a constructor
// "TypeError: Argument 2 of AudioWorkletGlobalScope.registerProcessor is not a constructor."
try {
  registerProcessor("sure!", () => {});
} catch (e) {
  console.log(e)
}

// Test empty name
// "NotSupportedError: Argument 1 of AudioWorkletGlobalScope.registerProcessor should not be an empty string."
try {
  registerProcessor("", EmptyWorkletProcessor);
} catch (e) {
  console.log(e)
}

// Test not an object
// "TypeError: Argument 2 of AudioWorkletGlobalScope.registerProcessor is not an object."
try {
  registerProcessor("my-worklet-processor", "");
} catch (e) {
  console.log(e)
}

// Test Empty class definition
// "TypeError: Argument 2 of AudioWorkletGlobalScope.registerProcessor constructor.process is not callable."
try {
  registerProcessor("empty-worklet-processor", EmptyWorkletProcessor);
} catch (e) {
  console.log(e)
}

// Test class with constructor but not process function
// "TypeError: Argument 2 of AudioWorkletGlobalScope.registerProcessor constructor.process is not callable."
try {
  registerProcessor("no-worklet-processor", NoProcessWorkletProcessor);
} catch (e) {
  console.log(e)
}

// Test class with parameterDescriptors not being array nor undefined
// "TypeError: Argument 2 of AudioWorkletGlobalScope.registerProcessor constructor.parameterDescriptors is neither an array nor undefined."
try {
  registerProcessor("bad-descriptors-worklet-processor", BadDescriptorsWorkletProcessor);
} catch (e) {
  console.log(e)
}

// Test class with good parameterDescriptors
// No error expected here
registerProcessor("good-descriptors-worklet-processor", GoodDescriptorsWorkletProcessor);

// Test class with constructor and process function
// No error expected here
registerProcessor("dummy-worklet-processor", DummyProcessWorkletProcessor);

// Test class adding class with the same name twice
// "NotSupportedError: Operation is not supported: Argument 1 of AudioWorkletGlobalScope.registerProcessor is invalid: a class with the same name is already registered."
try {
  registerProcessor("dummy-worklet-processor", DummyProcessWorkletProcessor);
} catch (e) {
  console.log(e)
}
