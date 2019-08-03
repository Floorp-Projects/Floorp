var T = TypedObject;
x = T.biguint64;
var ST = new T.StructType({ x });
new ST({'x':0n});
