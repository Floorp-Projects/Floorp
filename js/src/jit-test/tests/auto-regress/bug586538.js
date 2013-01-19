// Binary: cache/js-dbg-32-f7cf2b7b7961-linux
// Flags: -m
//
Function("\
  for (var a = 0,i=0;i<10;++i) {\
    if (a % 3) {\
      return\
    }\
    function b() {}\
    b((function() {})([Infinity]))\
  }\
")()
