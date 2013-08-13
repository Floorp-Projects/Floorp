// |jit-test| error: TypeError

if (!this.hasOwnProperty("Type"))
  throw new TypeError();

var Vec3 = new ArrayType(float32, 3);
var Sprite = new ArrayType(Vec3, 3);
var mario = new Sprite();
mario[/\u00ee[]/] = new Vec3([1, 0, 0]);

