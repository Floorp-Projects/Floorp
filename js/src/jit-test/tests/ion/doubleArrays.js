
function testPushConvert() {
  var x = [];
  for (var i = 0; i < 10; i++)
    x.push(i + .5);
  for (var i = 0; i < 5; i++)
    x.push(i);
  var res = 0;
  for (var i = 0; i < x.length; i++)
    res += x[i];
  assertEq(res, 60);
}
testPushConvert();

var UnsafeSetElement = getSelfHostedValue("UnsafeSetElement");
function testUnsafeSetConvert() {
  var x = [0.5, 1.5, 2.1];
  for (var i = 0; i < 2000; i++)
    // try to ensure we JIT and hence inline
    UnsafeSetElement(x, 1, i);
  var res = 0;
  for (var i = 0; i < x.length; i++)
    res += x[i];
  assertEq(res, 2001.6);
}
testUnsafeSetConvert();

function testArrayInitializer() {
  var x = [.5,1.5,2.5,3];
  var res = 0;
  for (var i = 0; i < x.length; i++)
    res += x[i];
  assertEq(res, 7.5);
}
for (var i = 0; i < 5; i++)
  testArrayInitializer();

function testArrayConstructor() {
  var x = Array(.5,1.5,2.5,3);
  var res = 0;
  for (var i = 0; i < x.length; i++)
    res += x[i];
  assertEq(res, 7.5);
}
for (var i = 0; i < 5; i++)
  testArrayConstructor();

function addInt(a) {
  // inhibit ion
  try {
    a[0] = 10;
  } catch (e) {}
}

function testBaseline() {
  var x = Array(.5,1.5,2.5,3);
  addInt(x);
  var res = 0;
  for (var i = 0; i < x.length; i++)
    res += x[i];
  assertEq(res, 17);
}
for (var i = 0; i < 5; i++)
  testBaseline();
