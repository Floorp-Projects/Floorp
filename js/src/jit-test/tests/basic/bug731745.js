function h(s) {
  return eval(s)
}
f = h("(function(){function::arguments=[]})")
for (a in f()) {}
