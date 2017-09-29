# Building WebAssembly source and source maps

```
emcc average.c -O2 -g4 -o average.js -s WASM=1 -s EXPORTED_FUNCTIONS="['_average','_sum']"
```
