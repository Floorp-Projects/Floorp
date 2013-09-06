// |jit-test| error:Error

if (!this.hasOwnProperty("Type"))
  throw new Error();

new ArrayType(uint8, .0000000009);
