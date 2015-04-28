// |jit-test| error: InternalError

new Function(`
var TO = TypedObject;
var PointType = new TO.StructType({x: TO.int32, y: TO.int32 });
function testPoint() {
    var p = new PointType();
    var sub = Object.create(p);
    sub.x = 5;
    anonymous('minEmptyChunkCount')   
}
testPoint();
`)();
