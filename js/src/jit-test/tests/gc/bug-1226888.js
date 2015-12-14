if (!this.hasOwnProperty("TypedObject"))
  quit();

setJitCompilerOption('ion.forceinlineCaches', 1);
// Adapted from randomly chosen test: js/src/jit-test/tests/TypedObject/jit-write-references.js
with({}) {}
v = new new TypedObject.StructType({
    f: TypedObject.Any
})
gc();
function g() {
    v.f = {
        Object
    };
    v.f;
}
for (var i = 0; i < 9; i++) {
    g();
}
