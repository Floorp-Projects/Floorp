// |jit-test| error:Error

if (!this.hasOwnProperty("TypedObject"))
  throw new Error();

TypedObject.uint8.array(.0000000009);
