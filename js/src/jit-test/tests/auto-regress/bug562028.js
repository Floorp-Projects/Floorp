// Binary: cache/js-dbg-64-0723bab9f15d-linux
// Flags:
//
function f(code) {
  uneval(Function(code.replace(/\/\*DUPTRY\d+/,
    function(k) {
      n = k.substr(8)
      return g("try{}catch(e){}", n)
    }
  )))
}
function g(s, n) {
  if (n == 1) return s
  s2 = s + s
  r = n % 2
  d = (n - r) / 2;
  m = g(s2, d)
  return r ? m: m
}
f("if(/>/(\"\")){/*DUPTRY4968(u)}else if([]()){}")
