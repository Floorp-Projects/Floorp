// |jit-test| error: TypeError: get length method
setJitCompilerOption("ic.force-megamorphic", 1);
function f(o) {
  return o.length;
}
f(new Int8Array(4));
f(Object.create(new Uint8Array()));
