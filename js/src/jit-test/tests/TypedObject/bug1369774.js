if (!this.hasOwnProperty("TypedObject"))
    quit();
var T = TypedObject;
var S = new T.StructType({f: T.Object});
var o = new S();
for (var i = 0; i < 15; i++)
    o.f = null;
