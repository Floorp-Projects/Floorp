if (!this.hasOwnProperty("TypedObject"))
  quit();

var Uint32Array = TypedObject.float32.array(3);
const ONE_MINUS_EPSILON = 1 - Math.pow(2, -53);
const f = new Float64Array([0, 0]);
const u = new Uint32Array(f.buffer);
const diff = function(a, b) {
    f[1] = b;
    u[3 - ENDIAN];
};
ENDIAN = 1;
diff(1, ONE_MINUS_EPSILON)
