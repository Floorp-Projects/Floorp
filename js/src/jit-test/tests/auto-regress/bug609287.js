// |jit-test| error:TypeError

// Binary: cache/js-dbg-32-7ec0a71652a6-linux
// Flags:
//
f = eval("\
  (function() {\
    __proto__ = \
    Proxy.createFunction((\
    function() {\
      return {\
        has: new ArrayBuffer,\
      }\
    })\
    (\"\"), \
    JSON.parse\
    )\
  })\
")()
