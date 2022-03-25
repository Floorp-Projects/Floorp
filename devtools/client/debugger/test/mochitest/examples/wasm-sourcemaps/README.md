# Building WebAssembly source with source maps

First download and install WASI SDK:
https://github.com/WebAssembly/wasi-sdk/blob/47e5865191c02a8943a1ce2dfb202167219435b8/README.md
```
export WASI_VERSION=12
export WASI_VERSION_FULL=${WASI_VERSION}.0
wget https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-${WASI_VERSION}/wasi-sdk-${WASI_VERSION_FULL}-linux.tar.gz
tar xvf wasi-sdk-${WASI_VERSION_FULL}-linux.tar.gz

export WASI_SDK_PATH=`pwd`/wasi-sdk-${WASI_VERSION_FULL}
CC="${WASI_SDK_PATH}/bin/clang --sysroot=${WASI_SDK_PATH}/share/wasi-sysroot"
```

Then, build the wasm file + its source map file
```
$CC --target=wasm32-unknown-unknown-wasm -nostartfiles -nostdlib -Wl,-no-entry,--export=fib -g fib.c -o fib.wasm

python3 wasm-sourcemap.py fib.wasm -o fib.wasm.map --dwarfdump=$WASI_SDK_PATH/bin/llvm-dwarfdump --source-map-url=https://example.com/browser/devtools/client/debugger/test/mochitest/examples/wasm-sourcemaps/fib.wasm.map -w fib.wasm
```
