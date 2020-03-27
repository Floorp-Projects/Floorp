// Define several classes.
class EmptyWorkletProcessor extends AudioWorkletProcessor {}

class NoProcessWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }
}

class BadDescriptorsWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process() {
    // Do nothing, output silence
  }

  static get parameterDescriptors() {
    return "A string";
  }
}

class GoodDescriptorsWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process() {
    // Do nothing, output silence
  }

  static get parameterDescriptors() {
    return [
      {
        name: "myParam",
        defaultValue: 0.707,
      },
    ];
  }
}

class DummyProcessWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process() {
    // Do nothing, output silence
  }
}

class DescriptorsNoNameWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process() {
    // Do nothing, output silence
  }

  static get parameterDescriptors() {
    return [
      {
        defaultValue: 0.707,
      },
    ];
  }
}

class DescriptorsDefaultValueNotNumberWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process() {
    // Do nothing, output silence
  }

  static get parameterDescriptors() {
    return [
      {
        name: "test",
        defaultValue: "test",
      },
    ];
  }
}

class DescriptorsMinValueNotNumberWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process() {
    // Do nothing, output silence
  }

  static get parameterDescriptors() {
    return [
      {
        name: "test",
        minValue: "test",
      },
    ];
  }
}

class DescriptorsMaxValueNotNumberWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process() {
    // Do nothing, output silence
  }

  static get parameterDescriptors() {
    return [
      {
        name: "test",
        maxValue: "test",
      },
    ];
  }
}

class DescriptorsDuplicatedNameWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process() {
    // Do nothing, output silence
  }

  static get parameterDescriptors() {
    return [
      {
        name: "test",
      },
      {
        name: "test",
      },
    ];
  }
}

class DescriptorsNotDictWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process() {
    // Do nothing, output silence
  }

  static get parameterDescriptors() {
    return [42];
  }
}

class DescriptorsOutOfRangeMinWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process() {
    // Do nothing, output silence
  }

  static get parameterDescriptors() {
    return [
      {
        name: "test",
        defaultValue: 0,
        minValue: 1,
        maxValue: 2,
      },
    ];
  }
}

class DescriptorsOutOfRangeMaxWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process() {
    // Do nothing, output silence
  }

  static get parameterDescriptors() {
    return [
      {
        name: "test",
        defaultValue: 3,
        minValue: 1,
        maxValue: 2,
      },
    ];
  }
}

class DescriptorsBadRangeMaxWorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
  }

  process() {
    // Do nothing, output silence
  }

  static get parameterDescriptors() {
    return [
      {
        name: "test",
        defaultValue: 1.5,
        minValue: 2,
        maxValue: 1,
      },
    ];
  }
}

// Test not a constructor
// "TypeError: Argument 2 of AudioWorkletGlobalScope.registerProcessor is not a constructor."
try {
  registerProcessor("sure!", () => {});
} catch (e) {
  console.log(e);
}

// Test empty name
// "NotSupportedError: Argument 1 of AudioWorkletGlobalScope.registerProcessor should not be an empty string."
try {
  registerProcessor("", EmptyWorkletProcessor);
} catch (e) {
  console.log(e);
}

// Test not an object
// "TypeError: Argument 2 of AudioWorkletGlobalScope.registerProcessor is not an object."
try {
  registerProcessor("my-worklet-processor", "");
} catch (e) {
  console.log(e);
}

// Test Empty class definition
registerProcessor("empty-worklet-processor", EmptyWorkletProcessor);

// Test class with constructor but not process function
registerProcessor("no-worklet-processor", NoProcessWorkletProcessor);

// Test class with parameterDescriptors being iterable, but the elements are not
// dictionaries.
// "TypeError: AudioWorkletGlobalScope.registerProcessor: Element 0 in parameterDescriptors can't be converted to a dictionary.",
try {
  registerProcessor(
    "bad-descriptors-worklet-processor",
    BadDescriptorsWorkletProcessor
  );
} catch (e) {
  console.log(e);
}

// Test class with good parameterDescriptors
// No error expected here
registerProcessor(
  "good-descriptors-worklet-processor",
  GoodDescriptorsWorkletProcessor
);

// Test class with constructor and process function
// No error expected here
registerProcessor("dummy-worklet-processor", DummyProcessWorkletProcessor);

// Test class adding class with the same name twice
// "NotSupportedError: Operation is not supported: Argument 1 of AudioWorkletGlobalScope.registerProcessor is invalid: a class with the same name is already registered."
try {
  registerProcessor("dummy-worklet-processor", DummyProcessWorkletProcessor);
} catch (e) {
  console.log(e);
}

// "name" is a mandatory field in descriptors
// "TypeError: Missing required 'name' member of AudioParamDescriptor."
try {
  registerProcessor(
    "descriptors-no-name-worklet-processor",
    DescriptorsNoNameWorkletProcessor
  );
} catch (e) {
  console.log(e);
}

// "defaultValue" should be a number
// "TypeError: 'defaultValue' member of AudioParamDescriptor is not a finite floating-point value."
try {
  registerProcessor(
    "descriptors-default-value-not-number-worklet-processor",
    DescriptorsDefaultValueNotNumberWorkletProcessor
  );
} catch (e) {
  console.log(e);
}

// "min" should be a number
// "TypeError: 'minValue' member of AudioParamDescriptor is not a finite floating-point value."
try {
  registerProcessor(
    "descriptors-min-value-not-number-worklet-processor",
    DescriptorsMinValueNotNumberWorkletProcessor
  );
} catch (e) {
  console.log(e);
}

// "max" should be a number
// "TypeError: 'maxValue' member of AudioParamDescriptor is not a finite floating-point value."
try {
  registerProcessor(
    "descriptors-max-value-not-number-worklet-processor",
    DescriptorsMaxValueNotNumberWorkletProcessor
  );
} catch (e) {
  console.log(e);
}

// Duplicated values are not allowed for "name"
// "NotSupportedError: Duplicated name \"test\" in parameterDescriptors"
try {
  registerProcessor(
    "descriptors-duplicated-name-worklet-processor",
    DescriptorsDuplicatedNameWorkletProcessor
  );
} catch (e) {
  console.log(e);
}

// Descriptors' elements should be dictionnary
// "TypeError: Element 0 in parameterDescriptors can't be converted to a dictionary.",
try {
  registerProcessor(
    "descriptors-not-dict-worklet-processor",
    DescriptorsNotDictWorkletProcessor
  );
} catch (e) {
  console.log(e);
}

// defaultValue value should be in range [minValue, maxValue]. defaultValue < minValue is not allowed
// "NotSupportedError: In parameterDescriptors, test defaultValue is out of the range defined by minValue and maxValue.",
try {
  registerProcessor(
    "descriptors-out-of-range-min-worklet-processor",
    DescriptorsOutOfRangeMinWorkletProcessor
  );
} catch (e) {
  console.log(e);
}

// defaultValue value should be in range [minValue, maxValue]. defaultValue > maxValue is not allowed
// "NotSupportedError: In parameterDescriptors, test defaultValue is out of the range defined by minValue and maxValue.",
try {
  registerProcessor(
    "descriptors-out-of-range-max-worklet-processor",
    DescriptorsOutOfRangeMaxWorkletProcessor
  );
} catch (e) {
  console.log(e);
}

// We should have minValue < maxValue to define a valid range
// "NotSupportedError: In parameterDescriptors, test minValue should be smaller than maxValue.",
try {
  registerProcessor(
    "descriptors-bad-range-max-worklet-processor",
    DescriptorsBadRangeMaxWorkletProcessor
  );
} catch (e) {
  console.log(e);
}
