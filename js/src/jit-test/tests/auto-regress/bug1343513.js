// |jit-test| error:RangeError
var i = 0;
do {
    i++;
    var ta = new Int32Array(inIon() ? 0x7fffffff : 1);
} while (!inIon());
