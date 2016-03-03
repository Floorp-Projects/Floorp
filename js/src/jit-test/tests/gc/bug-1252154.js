// Bug 1252154: Inline typed array objects need delayed metadata collection.
// Shouldn't crash.

gczeal(7,1);
enableShellObjectMetadataCallback();
var T = TypedObject;
var AT = new T.ArrayType(T.Any,10);
var v = new AT();
