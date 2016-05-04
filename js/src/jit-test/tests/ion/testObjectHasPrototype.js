setJitCompilerOption("ion.warmup.trigger", 4);

var ObjectHasPrototype = getSelfHostedValue("ObjectHasPrototype");

var StringProto = String.prototype;
var ObjectProto = Object.prototype;

function testBasic() {
  var f = function() {
    assertEq(ObjectHasPrototype(StringProto, ObjectProto), true);
  };
  for (var i = 0; i < 40; i++) {
    f();
  }
}
testBasic();

function testProtoChange(proto) {
  var f = function(expected) {
    assertEq(ObjectHasPrototype(StringProto, ObjectProto), expected);
  };
  var expected = true;
  for (var i = 0; i < 120; i++) {
    f(expected);
    if (i == 40) {
      Object.setPrototypeOf(StringProto, proto);
      expected = false;
    }
    if (i == 80) {
      Object.setPrototypeOf(StringProto, ObjectProto);
      expected = true;
    }
  }
}
testProtoChange(null);
// Different singleton
testProtoChange(Function.prototype);
// native non-singleton
testProtoChange(/a/);
// non-native non-singleton
testProtoChange({});

var Int32ArrayProto = Int32Array.prototype;
var TypedArrayProto = Object.getPrototypeOf(Int32ArrayProto);
function testProtoProtoChange(proto) {
  var f = function() {
    assertEq(ObjectHasPrototype(Int32ArrayProto, TypedArrayProto), true);
  };
  for (var i = 0; i < 120; i++) {
    f();
    if (i == 40)
      Object.setPrototypeOf(TypedArrayProto, proto);
    if (i == 80)
      Object.setPrototypeOf(TypedArrayProto, Object);
  }
}
testProtoProtoChange(null);
// Different singleton
testProtoProtoChange(Function.prototype);
// native non-singleton
testProtoProtoChange(/a/);
// non-native non-singleton
testProtoProtoChange({});
