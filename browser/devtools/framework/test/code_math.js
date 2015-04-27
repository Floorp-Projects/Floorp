function add(a, b, k) {
  var result = a + b;
  return k(result);
}
