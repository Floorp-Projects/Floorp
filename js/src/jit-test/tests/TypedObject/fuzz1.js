// |jit-test| error:RangeError

function* eval() {
    yield(undefined)
}
new TypedObject.StructType();
eval();
