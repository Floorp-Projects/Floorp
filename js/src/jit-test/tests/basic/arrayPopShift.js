
/* Test pop/shift compiler paths. */

function a() {
  var x = [];
  for (var i = 0; i < 50; i++)
    x.push(i);
  for (var j = 0; j < 100; j++) {
    var z = x.shift();
    if (j < 50)
      assertEq(z, j);
    else
      assertEq(z, undefined);
  }
}
a();

function b() {
  var x = [];
  for (var i = 0; i < 50; i++)
    x.push(i);
  for (var j = 0; j < 100; j++) {
    var z = x.pop();
    if (j < 50)
      assertEq(z, 49 - j);
    else
      assertEq(z, undefined);
  }
}
b();
