
x = [1,2,3];
setObjectMetadata(x, {y:0});
assertEq(getObjectMetadata(x).y, 0);

setObjectMetadataCallback(true);

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

var wc = getObjectMetadata(w).index;
var xc = getObjectMetadata(x).index;
var yc = getObjectMetadata(y).index;
var zc = getObjectMetadata(z).index;

assertEq(xc > wc, true);
assertEq(yc > xc, true);
assertEq(zc > yc, true);
assertEq(getObjectMetadata(x).stack[0], callee);
assertEq(getObjectMetadata(x).stack[1], hello);
