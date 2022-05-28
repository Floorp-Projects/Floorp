// |jit-test| skip-if: !wasmSimdEnabled()

// Do not include these in the preamble, they must be loaded after lib/wasm.js
load(scriptdir + "ad-hack-preamble.js")
load(scriptdir + "ad-hack-binop-preamble.js")

runSimpleBinopTest(0, 3);
