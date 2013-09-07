// |jit-test| error:RangeError;

if (!this.hasOwnProperty("Type"))
  throw new RangeError();

this.__proto__ =  Proxy.create({});
new StructType;
