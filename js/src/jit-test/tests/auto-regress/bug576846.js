// |jit-test| need-for-each

// Binary: cache/js-dbg-32-55f39d8d866c-linux
// Flags: -j
//
eval("\
  for each(d in[0,0,0,0,0,0,0,0,0,0,0,0]) {\
    (function f(aaaaaa,bbbbbb){\
      return aaaaaa.length==bbbbbb?0:aaaaaa[bbbbbb]+f(aaaaaa,bbbbbb+1)\
    })\
    ([,,true,'',,(0),(0/0),new Number,true,Number()],0)\
  }\
")
