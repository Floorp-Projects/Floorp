#!/bin/bash
X="var d = Date.now();";
for i in t/*.js; do X="$X load(\"$i\");"; done
X="$X print(Date.now() - d);"
echo $X | $1 -j
