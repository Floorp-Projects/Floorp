var f = function(i,j) { return i%j|0 };
assertEq(f(10, 0), 0);
var f = function(i,j) { return i%j|0 };
assertEq(f(0, 0), 0);
var f = function(i,j) { return i%j|0 };
assertEq(f(-Math.pow(2,31), -1), 0);
var f = function(i,j) { return i%j|0 };
assertEq(f(-4, 4), 0);

var f = function(i) { return i%4|0 };
assertEq(f(-4), 0);
var f = function(i) { return i%4|0 };
assertEq(f(0), 0);

var f = function(i) { return i%3|0 };
assertEq(f(-3), 0);
var f = function(i) { return i%3|0 };
assertEq(f(0), 0);
