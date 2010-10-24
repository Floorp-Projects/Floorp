// |trace-test| error: TypeError
function f() {
  eval("(function() \n{\nfor(x in[])\n{}\n})");
  ("")()
}
f()

