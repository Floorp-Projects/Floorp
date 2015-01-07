
if (typeof TypedObject === "undefined")
  quit();

var int32x4 = SIMD.int32x4;
var a = int32x4((4294967295), 200, 300, 400);
addCase( new Array(Math.pow(2,12)) );
for ( var arg = "", i = 0; i < Math.pow(2,12); i++ ) {}
addCase( a );
function addCase(object) {
  object.length 
}
