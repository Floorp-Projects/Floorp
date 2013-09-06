// |jit-test| error:TypeError

if (!this.hasOwnProperty("Type"))
  throw new TypeError();

new StructType(RegExp);
