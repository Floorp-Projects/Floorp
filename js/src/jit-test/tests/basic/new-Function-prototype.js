var funProto = Function.prototype;
assertEq(Object.getOwnPropertyDescriptor(funProto, "prototype"), undefined);
assertEq(parseInt.prototype, undefined);
var oldObj;
for (var i = 0, sz = RUNLOOP; i < sz; oldObj = obj, i++)
{

  try {
    var obj = new funProto;
  }
  catch (e) {}
  assertEq(Object.getOwnPropertyDescriptor(funProto, "prototype"), undefined);
  assertEq(Object.getOwnPropertyDescriptor(parseInt, "prototype"), undefined);
  assertEq(parseInt.prototype, undefined);
}
checkStats({
  recorderStarted: 1,
  recorderAborted: 0,
  traceTriggered: 1,
  traceCompleted: 1,
});
