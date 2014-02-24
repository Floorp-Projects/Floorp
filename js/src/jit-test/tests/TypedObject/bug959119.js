// This test exposed a bug in float32 optimization.
// The (inlined and optimized) code for `add()` created
// MDiv instructions specialized to integers, which was
// then "respecialized" to float32, leading to internal
// assertion errors.
//
// Public domain.

if (!this.hasOwnProperty("TypedObject"))
  quit();

var {StructType,uint8,float32} = TypedObject;
var RgbColor2 = new StructType({r: uint8, g: float32, b: uint8});
RgbColor2.prototype.add = function(c) {
  this.g += c;
  this.b += c;
};
var gray = new RgbColor2({r: 129, g: 128, b: 127});
gray.add(1);
gray.add(2);

