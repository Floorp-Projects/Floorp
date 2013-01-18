// |jit-test| error:TypeError

// Binary: cache/js-dbg-32-805fd625e65f-linux
// Flags: -j
//
x = Proxy.create((function () {
    return {
        get: function () {}
    }
}()), Object.e)
Function("\
  for(var a = 0; a < 2; ++a) {\
    if (a == 0) {}\
    else {\
      x > x\
    }\
  }\
")()
