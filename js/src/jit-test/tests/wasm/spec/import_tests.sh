#!/bin/bash

rm -rf ./*.wast ./list.js ./*.wast.js

git clone https://github.com/WebAssembly/spec spec
mv spec/ml-proto/test/*.wast ./
rm -rf spec/

for i in $(ls *.wast);
do
    echo "// |jit-test| test-also-wasm-baseline" > $i.js
    echo "var importedArgs = ['$i']; load(scriptdir + '../spec.js');" >> $i.js
done;
