function f() {
  for(var i=0; i<3; i++) {
    var x = -i / 100;
    assertEq(x * -100, i);
  }
}
f();

function g() {
  for (var i = 0; i < 2; i++) {
    var a = i ? true : false;
    var x = -a / 100;
    assertEq(x * -100, i);
  }
}
g();

function h() {
  for (var i = 0; i < 20; i++)
    var x = (0x7ffffff4 + i) / 100;
  assertEq(x, 21474836.55);
}
h();
