
/* Don't crash. */

var count = 0;

function callbackfn(v) {
  if (++count == 98)
    count = 0x7ffffff0;
  return arr[0] + count;
}

function foo() {
  arr = [1,2,3,4,5];
  for (var i = 0; i < 50; i++)
    arr = arr.map(callbackfn);
}
foo();

function f(a,b,c) {
    a = 1; b = 'str'; c = 2.1;
    return arguments[0];
}
for (var i = 0; i < 20; i++)
  assertEq(f(10,'sss',1), 1);
