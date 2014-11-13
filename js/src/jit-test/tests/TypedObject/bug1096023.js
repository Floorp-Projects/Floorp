if (!this.hasOwnProperty("TypedObject"))
  quit();

enableSPSProfiling();
var T = TypedObject;
function check(results, ctor) {
  for (var i = 0; i < results.length; i++)
  var S = new T.StructType({f: ctor});
  for (var i = 0; i < results.length; i++) {
    var s = new S({f: results[i][1]});
  }
}
var int8results = [
  [22, 22],
  [-128, 128],
  [-1, 255],
  [0x75, 0x7575],
  [-123, 0x7585]
];
check(int8results, T.int8);
