var a = 2;
function getprop_inner(o2)
{
  var o = {a:5};
  var t = this;
  var x = 0;
  for (var i = 0; i < 20; i++) {
    t = this;
    x += o.a + o2.a + this.a + t.a;
  }
  return x;
}
function getprop() {
  return getprop_inner({a:9});
}
assertEq(getprop(), 360);
