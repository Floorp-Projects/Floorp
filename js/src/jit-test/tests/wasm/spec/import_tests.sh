#!/bin/bash

rm -rf ./*.wast ./*.wast.js

git clone https://github.com/WebAssembly/spec spec
mv spec/interpreter/test/*.wast ./
rm -rf spec/

# TODO not handled yet
rm -f *.fail.wast

for i in $(ls *.wast);
do
    echo "var importedArgs = ['$i']; load(scriptdir + '../wast.js');" > $i.js
done;
