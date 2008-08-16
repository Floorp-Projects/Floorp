#!/bin/bash
T="0"
for i in t/*.js; do 
	T+="+"
	T+=`Darwin_OPT.OBJ/js -j -e 'var d = Date.now(); load("'$i'"); print(Date.now() - d);'`
	 
done
echo $T | bc

