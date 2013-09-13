function blackboxme(fn) {
  (function one() {
    (function two() {
      (function three() {
        fn();
      }());
    }());
  }());
}
