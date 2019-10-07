// |jit-test| skip-if: !this.hasOwnProperty("TypedObject")

function f(obj, x, y) {
  // The assignment mustn't use the same registers as |x| or |y| for temporaries.
  obj.foo = y;

  // Ensure the assignment didn't clobber any registers used by either |x| or |y|.
  assertEq(x, 0.1);
  assertEq(y, 0.2);
}

// Different struct types to ensure we emit a SetProp IC.
var objects = [
  new new TypedObject.StructType({foo: TypedObject.float64}),
  new new TypedObject.StructType({foo: TypedObject.float32}),
];

// Load from an array to ensure we don't mark the values as constant.
var xs = [0.1, 0.1];
var ys = [0.2, 0.2];

for (var i = 0; i < 100; ++i) {
  f(objects[i & 1], xs[i & 1], ys[i & 1]);
}
