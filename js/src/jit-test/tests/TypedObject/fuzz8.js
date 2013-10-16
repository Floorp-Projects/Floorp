// |jit-test| error:Error

if (!this.hasOwnProperty("TypedObject"))
  throw new Error();

new TypedObject.ArrayType(TypedObject.uint8, .0000000009);
