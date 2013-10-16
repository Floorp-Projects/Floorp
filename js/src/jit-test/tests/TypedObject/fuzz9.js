// |jit-test| error: TypeError

if (!this.hasOwnProperty("TypedObject"))
  throw new TypeError();

var Vec3 = new TypedObject.ArrayType(TypedObject.float32, 3);
var Sprite = new TypedObject.ArrayType(Vec3, 3);
var mario = new Sprite();
mario[/\u00ee[]/] = new Vec3([1, 0, 0]);

