if (!this.hasOwnProperty("TypedObject"))
  quit();

this.__defineGetter__("x",
  function() {
    return this;
  }
);
function callback(obj) {}
enableShellAllocationMetadataBuilder();
evaluate("\
var { ArrayType, StructType, uint32 } = TypedObject;\
  var L = 1024;\
  var Matrix = uint32.array(L, 2);\
  var matrix = new Matrix();\
  for (var i = 0; i < L; i++)\
    matrix[i][0] = x;\
", { isRunOnce : true });
