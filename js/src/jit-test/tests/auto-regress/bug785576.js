// Binary: cache/js-dbg-64-29ca472bf2d2-linux
// Flags: -m -n -a
//
sandbox = newGlobal('')
evalcx("x=[]", sandbox)
evalcx("\
  x[0] = this;\
  Object.defineProperty(x, 0, {})\
", sandbox)
gc()
evalcx("x.shift()", sandbox)
