// |jit-test| error: TypeError
t = ""
function f(code) {
  eval("(function(){" + code + "})")()
}
evalcx("")
f("var r=({a:1})[\"\"];t(r)")
