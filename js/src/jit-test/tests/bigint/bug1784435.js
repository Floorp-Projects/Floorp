// |jit-test| error:RangeError
v2 = new Uint32Array(65537);
v4 = Object.getOwnPropertyNames(v2);
do {} while (v4 < 10n);
