if (typeof TypedObject === "undefined")
  quit();

load(libdir + "asserts.js")

var {StructType, ArrayType, uint32, Object,
     Any, storage, objectType} = TypedObject;

var anArray = new Uint32Array(100);
anArray[0] = 22;
var buffer = anArray.buffer;

var Unit = new StructType(({ f : Object, } ));
assertThrowsInstanceOf(() => new Unit(buffer), TypeError,
                       "Able to instantiate opaque type atop buffer");

var Units = new ArrayType(Unit, 2);
assertThrowsInstanceOf(() => new Units(buffer), TypeError,
                       "Able to instantiate opaque type atop buffer");
