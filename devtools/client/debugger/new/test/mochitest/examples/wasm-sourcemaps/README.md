# Building WebAssembly source and source maps

```
# using https://github.com/yurydelendik/wasmception to build wasm
$wasminception/dist/bin/clang --target=wasm32-unknown-unknown-wasm \
  --sysroot $wasminception/bin/clang -nostartfiles -nostdlib \
  -Wl,-no-entry,--export=fib -g fib.c -o fib.debug.wasm

# remove dwarf sections and build map file
wasm-dwarf fib.debug.wasm -o fib.wasm.map -s -p "`pwd`=wasm-src://" -x -w fib.wasm
```
