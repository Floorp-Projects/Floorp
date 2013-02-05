// |jit-test| error: TypeError
function f(c) {
  var b = arguments;
  if (c == 1)
    b = 1;
  return b;
}

evaluate("f('a', 'b', 'c', 'd', 'e');");
function test(){
  var args = f('a', (0), 'c');
  for (var v of args)
      s += v;
}
test();
