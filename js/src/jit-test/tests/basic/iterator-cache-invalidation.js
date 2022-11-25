function test(obj, expected) {
  var result = "";
  for (var s in obj) {
    result += s + ",";
  }
  assertEq(result, expected);
}

function runTest(mutate, expectedAfter) {
  var p = {px: 1, py: 2};
  var o = Object.create(p);
  o.x = 3;
  o.y = 4;

  var expectedBefore = "x,y,px,py,";
  test(o, expectedBefore);
  mutate(o, p);
  test(o, expectedAfter);
}


function testAddElement() {
  runTest((o,p) => { o[0] = 5; }, "0,x,y,px,py,");
}
function testAddProtoElement() {
  runTest((o,p) => { p[0] = 5; }, "x,y,0,px,py,");
}
function testDelete() {
  runTest((o,p) => { delete o.x; }, "y,px,py,");
}
function testProtoDelete() {
  runTest((o,p) => { delete p.px; }, "x,y,py,");
}
function testMakeUnenumerable() {
  runTest((o,p) => {
    Object.defineProperty(o, "x", { value: 1, enumerable: false });
  }, "y,px,py,");
}
function testMakeProtoUnenumerable() {
  runTest((o,p) => {
    Object.defineProperty(p, "px", { value: 1, enumerable: false });
  }, "x,y,py,");
}

for (var i = 0; i < 10; i++) {
  testAddElement();
  testAddProtoElement();
  testDelete();
  testProtoDelete();
  testMakeUnenumerable()
  testMakeProtoUnenumerable()
}
