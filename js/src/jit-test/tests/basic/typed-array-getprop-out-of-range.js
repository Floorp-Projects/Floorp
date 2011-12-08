var r = Object.prototype[0] = /a/;

function test()
{
  for (var i = 0, sz = 9; i < sz; i++)
  {
    var ta = new Int8Array();
    assertEq(ta[0], r);
  }
}
test();
