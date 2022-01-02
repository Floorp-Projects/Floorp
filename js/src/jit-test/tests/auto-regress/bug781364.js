// Binary: cache/js-dbg-64-21b4797e4cb3-linux
// Flags: -m -a --ion-eager
//
k = m
function h() {
  switch (true) {
  default:
    x = newGlobal('')
  }
  return function(f, code) {
    try {
      evalcx(code, x)
    } catch (e) {}
  }
}
function m() {
  f()
}
function g(code) {
  f = new Function(code);
  k(f, code)
}
g("k=h()")
g("\
    a='';\
    Object.defineProperty(this,\"b\",{get:function(){a=this.d()}});\
    Object.defineProperty(this,\"c\",{get:function(){b}});\
    d=2;\
    b\
");
g("b");
g("b");
g("for(v of c);");
g("a=eval(\"function f(){}\");b")
