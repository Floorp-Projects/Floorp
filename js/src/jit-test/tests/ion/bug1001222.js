function f(x, y) {
  return +(x ? x : y), y >>> 0
}
f(0, -0)
f(0, 2147483649)
