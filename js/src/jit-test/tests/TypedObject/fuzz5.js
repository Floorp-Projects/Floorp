if (!this.hasOwnProperty("TypedObject"))
  quit();

var Color = new TypedObject.StructType({r: TypedObject.uint8,
                                        g: TypedObject.uint8,
                                        b: TypedObject.uint8});
var white2 = new Color({r: 255, toString: null, b: 253});

