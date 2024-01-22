function f(i) {
  // Add use of the boxed Int32 parameter |i| as a Double.
  (2 ** i);

  // This call into |g| will get inlined and an inline-arguments object will be
  // created. |i| was unboxed to Double earlier, but we must not store the
  // unboxed Double into the inline-arguments object, because other uses of the
  // boxed Int32 |i| may expect Int32 inputs.
  return g(i);
}

function g(i) {
  // Add use of aliased |i| as an Int32.
  if (i) {
    // Add use |arguments| to make |i| an aliased variable which gets stored in
    // the inline-arguments object.
    return arguments;
  }
}

// Don't inline |f| into the top-level script.
with ({});

for (let i = 0; i < 4000; i++) {
  f(i);
}
