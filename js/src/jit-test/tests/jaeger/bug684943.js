
function foo(x) {
  for (var i = 0; i < 100; i++) {
    x.f === i;
  }
}
foo({f:"three"});
