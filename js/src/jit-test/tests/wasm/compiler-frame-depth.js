// Ensures that the postorder allows us to have very deep expression trees.

var expr = '(get_local 0)';

for (var i = 1000; i --> 0; ) {
    expr = `(f32.neg ${expr})`;
}

var code = `(module
 (func
  (result f32)
  (param f32)
  ${expr}
 )
 (export "run" 0)
)`;

wasmFullPass(code, Math.fround(13.37), {}, 13.37);
