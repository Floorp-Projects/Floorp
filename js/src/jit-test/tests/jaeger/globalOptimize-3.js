
y = 30;
function bar() {
  assertEq(y, 30);
  Object.defineProperty(this, 'y', {writable:false});
  y = 10;
  assertEq(y, 30);
}
bar();

x = 30;
function foo() {
  assertEq(x, 30);
  Object.freeze(this);
  x = 10;
  assertEq(x, 30);
}
foo();
