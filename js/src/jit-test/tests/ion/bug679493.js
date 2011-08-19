// Normal checks:
function mul(x, y) {
    return x*y;
}
assertEq(mul(1, 2), 2);
assertEq(mul(0, 2), 0);
assertEq(mul(0, -1), -0);
assertEq(mul(100000000, 20000), 2000000000000);
assertEq(mul(0, -2), -0);
assertEq(mul(0, 0), 0);

// Constant * value checks:
assertEq(function(x){return x*5}(4), 20);
assertEq(function(x){return x*5}(0), 0);
assertEq(function(x){return x*5}(-4), -20);
assertEq(function(x){return x*0}(0), 0);
assertEq(function(x){return x*0}(5), 0);
assertEq(function(x){return x*0}(-5), -0);
assertEq(function(x){return x*-5}(4), -20);
assertEq(function(x){return x*-5}(0), -0);
assertEq(function(x){return x*-5}(-4), 20);
assertEq(function(x){return x*20000}(100000000), 2000000000000);

// Constant folding
assertEq(function(){var x=5; return x*4}(), 20);
assertEq(function(){var x=5; return x*-4}(), -20);
assertEq(function(){var x=0; return x*4}(), 0);
assertEq(function(){var x=0; return x*0}(), 0);
assertEq(function(){var x=0; return x*-4}(), -0);
assertEq(function(){var x=20000; return x*100000000}(), 2000000000000);
