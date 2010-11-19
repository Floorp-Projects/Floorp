
/* Don't confuse JIT code by making slow arrays that use inline slots inconsistently. */

function foo(a)
{
  assertEq(a.x, 5);
}

function bar()
{
  for (var i = 0; i < 50; i++) {
    var a = [];
    a[i] = 0;
    delete a[i];
    a.x = 5;
    foo(a);
  }

  var b = [1,,2,,3,,4,,5];
  assertEq(b.toString(), "1,,2,,3,,4,,5");
  b.x = 0;
  assertEq(b.toString(), "1,,2,,3,,4,,5");
  delete b.x;
  delete b[8];
  delete b[6];
  delete b[4];
  assertEq(b.toString(), "1,,2,,,,,,");
}
bar();
