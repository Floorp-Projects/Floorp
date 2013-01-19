// |jit-test| error:TypeError

// Binary: cache/js-dbg-32-89b775191b9d-linux
// Flags: -j
//
x = Proxy.create(function() {
  return {
    get: function() {}
  };
} ());
for (var a = 0; a < 6; ++a) {
  if (a == 3) {
    x > ""
  }
}
