# Source Maps

This package contains DevTools utilities for working with source maps.

This is used in multiple contexts:

* The debugger loads this package directly when running from the launchpad in a
  tab by itself
* The toolbox inside Firefox loads this package and passes it down to interested
  tools so that they can share a common instance of the utilities

# Application Requirements

This package assumes that an application using this code will make the
`worker.js`, and `dwarf_to_json.wasm` files available and call
`startSourceMapWorker(workerURL, wasmRoot)` to initialize the worker and specify
the location of the wasm asset.
