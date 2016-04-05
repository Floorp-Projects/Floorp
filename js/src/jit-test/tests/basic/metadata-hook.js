
enableShellAllocationMetadataBuilder();

function Foo() {
  this.x = 0;
  this.y = 1;
}

function hello() {
  function there() {
    w = new Foo();
    x = [1,2,3];
    y = [2,3,5];
    z = {a:0,b:1};
  }
  callee = there;
  callee();
}
hello();

var wc = getAllocationMetadata(w).index;
var xc = getAllocationMetadata(x).index;
var yc = getAllocationMetadata(y).index;
var zc = getAllocationMetadata(z).index;

assertEq(xc > wc, true);
assertEq(yc > xc, true);
assertEq(zc > yc, true);
assertEq(getAllocationMetadata(x).stack[0], callee);
assertEq(getAllocationMetadata(x).stack[1], hello);
