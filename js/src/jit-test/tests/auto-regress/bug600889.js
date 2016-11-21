// |jit-test| need-for-each

// Binary: cache/js-dbg-64-98c134cf59ef-linux
// Flags: -j
//
uneval = function(){}
Function("\
  function zz(aa) {\
    if (aa) this.a = decodeURIComponent;\
    gc();\
    delete this.a\
  }\
  for each(c in [0, 0, 0, 0, 0, 0, 0, new Boolean(false), \
                  0, new Boolean(false), new Boolean(false), \"\"]) {\
    l=new zz(c)\
  }\
")()
