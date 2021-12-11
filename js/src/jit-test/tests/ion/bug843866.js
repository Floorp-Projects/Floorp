function g(f) {}
function f(b) {
  g.apply(null, arguments);

  if (b < 10)
    f(b+1);
}
f(0);
