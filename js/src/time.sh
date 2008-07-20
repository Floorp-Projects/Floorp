#!/bin/sh
echo interp:
for i in 1 2 3 4 5; do
  Darwin_OPT.OBJ/js -e 'var d = Date.now(); load("'$1'"); print(Date.now() - d);'
done
echo jit:
for i in 1 2 3 4 5; do
  Darwin_OPT.OBJ/js -j -e 'var d = Date.now(); load("'$1'"); print(Date.now() - d);'
done
