if (!this.hasOwnProperty("TypedObject"))
  quit();

v = new new TypedObject.StructType({
    f: TypedObject.Any
})
gczeal(14);
var lfOffThreadGlobal = newGlobal();
