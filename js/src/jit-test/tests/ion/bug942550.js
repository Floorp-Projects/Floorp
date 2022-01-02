function pow(x,y) {
    return Math.pow(x,y);
}
var x = pow(3, -.5);
var y = pow(3, -.5);
assertEq(x, y);
