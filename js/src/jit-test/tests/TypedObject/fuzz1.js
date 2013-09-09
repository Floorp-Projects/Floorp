// |jit-test| error:RangeError

if (!this.hasOwnProperty("TypedObject"))
  throw new RangeError();

function eval() {
    yield(undefined)
}
new TypedObject.StructType();
eval();
