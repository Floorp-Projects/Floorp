function h(code) {
  f = Function(code);
  g()
}
function g() {
  f()
}
h()
h()
h("\
  arguments[\"0\"];\
  gc();\
")
