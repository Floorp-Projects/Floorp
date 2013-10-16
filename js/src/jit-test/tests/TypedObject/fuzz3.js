// |jit-test| error:RangeError;

if (!this.hasOwnProperty("TypedObject"))
  throw new RangeError();

this.__proto__ =  Proxy.create({});
new TypedObject.StructType;
