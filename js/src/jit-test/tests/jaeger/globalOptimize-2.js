
x = 30;
function foo() {
  assertEq(x, 30);
  delete x;
  y = 20;
  Object.defineProperty(this, 'x', {value:10});
  assertEq(x, 10);
}
foo();
