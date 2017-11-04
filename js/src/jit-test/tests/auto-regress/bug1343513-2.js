// |jit-test| error:RangeError
var i = 0;
do {
    i++;
    var ta = new Int32Array(inIon() ? 0x20000001 : 1);
} while (!inIon());
