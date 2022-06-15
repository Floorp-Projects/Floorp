// |jit-test| --fast-warmup
function* f() {
  try {
    yield;
  } finally {
    for (let i = 0; i < 10; i++) {}
  }
}
enableGeckoProfilingWithSlowAssertions();
for (var i = 0; i < 25; ++i) {
  let it = f();
  it.next();
  it.return();
}
