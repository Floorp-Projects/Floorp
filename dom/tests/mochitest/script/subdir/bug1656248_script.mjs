// Import a module which should be resolved relative to this script's URL.
import("./bug1656248_import.mjs")
  .then(ns => window.parent.checkResult(ns.default))
  .catch(e => window.parent.checkResult(`error: ${e}`));

// To trigger the bytecode cache, this script needs to be at least 1KB.
// To trigger the bytecode cache, this script needs to be at least 1KB.
// To trigger the bytecode cache, this script needs to be at least 1KB.
// To trigger the bytecode cache, this script needs to be at least 1KB.
// To trigger the bytecode cache, this script needs to be at least 1KB.
// To trigger the bytecode cache, this script needs to be at least 1KB.
// To trigger the bytecode cache, this script needs to be at least 1KB.
// To trigger the bytecode cache, this script needs to be at least 1KB.
// To trigger the bytecode cache, this script needs to be at least 1KB.
// To trigger the bytecode cache, this script needs to be at least 1KB.
// To trigger the bytecode cache, this script needs to be at least 1KB.
// To trigger the bytecode cache, this script needs to be at least 1KB.
// To trigger the bytecode cache, this script needs to be at least 1KB.
