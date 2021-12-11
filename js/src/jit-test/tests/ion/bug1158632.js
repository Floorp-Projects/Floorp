for (var j = 0; j < 1; ++j) {
  function f(x) {
    x = 4294967295 >>> 4294967295 % x
    switch (-1) {
      case 1:
      // case 0:
      case -1:
        x = 0;
      // default:
    }
  }
  f();
}
