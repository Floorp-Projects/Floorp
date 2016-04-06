// Bug 1252154: Inline typed array objects need delayed metadata collection.
// Shouldn't crash.

if (!this.hasOwnProperty("TypedObject"))
  quit();

gczeal(7,1);
enableShellAllocationMetadataBuilder();
var T = TypedObject;
var AT = new T.ArrayType(T.Any,10);
var v = new AT();
