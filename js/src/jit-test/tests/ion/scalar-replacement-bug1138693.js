// |jit-test| skip-if: !this.hasOwnProperty("TypedObject")

var T = TypedObject;
var ST = new T.StructType({x:T.int32});
function check(v) {
    return v.equivalent(T.int32);
}
function test() {
    var fake = { equivalent: ST.equivalent };
    try {
        check(fake);
    } catch (e) {}
}
test();
var uint8 = TypedObject.uint8;
function runTests() {
  uint8.equivalent(T.int32);
}
runTests();
test();

