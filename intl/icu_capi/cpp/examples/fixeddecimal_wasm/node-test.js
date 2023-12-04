wasm = require("./node-version.js");

const exitCode = wasm.runFixedDecimal();
if (exitCode !== 0) {
  throw new Error(`Test failed with exit code ${exitCode}`)
}
