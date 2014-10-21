// Binary: cache/js-dbg-64-50e4ff05741e-linux
// Flags: -m -n -a
//
gc()
var p = n
function m() {
  return function(f, code, t) {
	try {
	  evalcx(code, newGlobal())
	} catch (e) {}
  }
}
function n() {
	f()
}
function h(code) {
	f = Function(code)
	p(f, code, true)
}
h("\
  p=m();\
  gcPreserveCode();\
  gcslice(8);\
")
h("\"\"")
h("")
h("gc()")
h("")
h("")
h("gczeal(4,2)")
