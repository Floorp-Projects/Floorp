// |jit-test| slow; skip-if: getBuildConfiguration("debug")

var s = ''; 
s += new Uint8Array(2 ** 23 + 2); 
eval("[" + s + "1]");
