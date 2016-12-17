// This should work for real... at some point.
registerProcessor("sure!", () => {});
console.log(this instanceof AudioWorkletGlobalScope ? "So far so good" : "error");
