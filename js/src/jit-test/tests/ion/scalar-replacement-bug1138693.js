if (!this.hasOwnProperty("TypedObject"))
  quit();

var T = TypedObject;
var ST = new T.StructType({x:T.int32});
function check(v) {
    return v.toSource();
}
function test() {
    var fake = { toSource: ST.toSource };
    try {
        check(fake);
    } catch (e) {}
}
test();
var uint8 = TypedObject.uint8;
function runTests() {
  uint8.toSource();
}
runTests();
test();

