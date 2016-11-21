// |jit-test| need-for-each

// Binary: cache/js-dbg-32-33b05dd43cd4-linux
// Flags: -j
//
(function() {
  for each(let z in [0, 0, 0, 0]) {
    eval("\
      for(var a = 0; a < 1; ++a) {\
        this\
      }\
    ")
  }
})()
