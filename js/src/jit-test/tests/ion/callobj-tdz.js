// CallObject with TDZ arguments
function test(
  a00, a01, a02, a03, a04, a05, a06, a07, a08, a09,
  a10, a11, a12, a13, a14, a15, a16, a17, a18, a19,
  {} = () => {
    with ({}) {};
    return a00;
  }) {
}

// Warm-up JITs
for (var i = 0; i < 2000; ++i) {
  test();
}
