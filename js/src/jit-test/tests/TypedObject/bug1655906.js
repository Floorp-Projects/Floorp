// |jit-test| skip-if: !this.hasOwnProperty("TypedObject")
var S = new TypedObject.StructType({foo: TypedObject.bigint64});
var to = new S();
function g(b) {
    to.foo = !b;
}
function f() {
    for (var i = 0; i < 1500; i++) {
        g(i < 1000);
    }
}
f();
