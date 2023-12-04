function foo(glob, imp, b) {
    "use asm";
    var arr=new glob.Int8Array(b);
    return {};
  }
  a = new ArrayBuffer(64 * 1024);
  foo(this, null, a);
  function f(h, g) {
    ensureNonInline(g);
  }
f(Float64Array, a);
  