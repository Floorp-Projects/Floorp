function testNewWithNonNativeProto()
{
  function f() { }
  var a = f.prototype = [];
  for (var i = 0; i < 5; i++)
    var o = new f();
  return Object.getPrototypeOf(o) === a && o.splice === Array.prototype.splice;
}
assertEq(testNewWithNonNativeProto(), true);
