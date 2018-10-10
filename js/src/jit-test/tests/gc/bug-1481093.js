// |jit-test| skip-if: !this.hasOwnProperty("TypedObject")

v = new new TypedObject.StructType({
    f: TypedObject.Any
})
gczeal(14);
var lfOffThreadGlobal = newGlobal();
