// |jit-test| error:TypeError; skip-if: !this.hasOwnProperty("TypedObject")

gczeal(14, 8);
try {
  badReference();
} catch (exc) {}

var ObjectStruct = new TypedObject.StructType({});
var ValueStruct = new TypedObject.StructType({
    field: TypedObject.Any
});

ObjectStruct.field2 = new ValueStruct();

undefined.toString();
