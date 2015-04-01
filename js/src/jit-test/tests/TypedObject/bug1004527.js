if (!this.hasOwnProperty("TypedObject"))
  quit();

var { ArrayType, StructType, uint32 } = TypedObject;
var L = 1024;
var Matrix = uint32.array(L, 2);
var matrix = new Matrix();
evaluate("for (var i = 0; i < L; i++) matrix[i][0] = (function d() {});",
	 { isRunOnce: true });
