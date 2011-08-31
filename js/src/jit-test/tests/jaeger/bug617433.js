// don't crash

function foo(x) {
  (x >>> 3.14);
  (x >>> true);
  (x >>> (0/0));
  (x >>> 100);
  (x >>> -10);
  (x >>> (1/0));
  (x >>> (void 0));
}
foo(10);
