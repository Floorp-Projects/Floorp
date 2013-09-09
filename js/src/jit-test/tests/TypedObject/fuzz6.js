// |jit-test| error:TypeError

if (!this.hasOwnProperty("TypedObject"))
  throw new TypeError();

new TypedObject.StructType(RegExp);
