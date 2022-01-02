load(libdir + "./asserts.js");

// Exercise object-literal creation. Many other tests also exercise details of
// objects created by object literals -- e.g., byteSize-of-objects.js. Here we
// simply want to hit the cases {run-once ctx, repeated ctx} x {constant,
// parameterized} x {small, large}.

function build_large_literal(var_name, num_keys, extra) {
  let s = "var " + var_name + " = {";
  for (let i = 0; i < num_keys; i++) {
    s += "prop" + i + ": " + i + ",";
  }
  s += extra;
  s += "};";
  return s;
}

let small_singleton = {a: 1, b: 2, 0: "I'm an indexed property" };
// Large enough to force dictionary mode -- should inhibit objliteral use in
// frontend:
eval(build_large_literal("large_singleton", 513, ""));

let an_integer = 42;
let small_singleton_param = { a: 1, b: 2, c: an_integer };
eval(build_large_literal("large_singleton_param", 513, "prop_int: an_integer"));

function f(a_parameterized_integer) {
  let small_templated = {a: 1, b: 2, 0: "I'm an indexed property" };
  // Large enough to force dictionary mode -- should inhibit objliteral use in
  // frontend:
  eval(build_large_literal("large_templated", 513, ""));

  let small_templated_param = { a: 1, b: 2, c: a_parameterized_integer };
  eval(build_large_literal("large_templated_param", 513, "prop_int: a_parameterized_integer"));

  return {small_templated, large_templated,
          small_templated_param, large_templated_param};
}

for (let i = 0; i < 10; i++) {
  let {small_templated, large_templated,
       small_templated_param, large_templated_param} = f(42);

  assertDeepEq(small_templated, small_singleton);
  assertDeepEq(large_templated, large_singleton);
  assertDeepEq(small_templated_param, small_singleton_param);
  assertDeepEq(large_templated_param, large_singleton_param);
}

let small_lit_array = [0, 1, 2, 3];
let large_cow_lit_array = [0, 1, 2, 3, 4, 5, 6, 7];
assertEq(4, small_lit_array.length);
assertEq(8, large_cow_lit_array.length);
for (let i = 0; i < small_lit_array.length; i++) {
  assertEq(i, small_lit_array[i]);
}
for (let i = 0; i < large_cow_lit_array.length; i++) {
  assertEq(i, large_cow_lit_array[i]);
}
