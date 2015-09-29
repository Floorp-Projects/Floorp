// |jit-test| error:RangeError;
load(libdir + "immutable-prototype.js");

if (!this.hasOwnProperty("TypedObject"))
  throw new RangeError();

if (globalPrototypeChainIsMutable())
  this.__proto__ =  Proxy.create({});

new TypedObject.StructType;
