
/* Non-reentrant closure used in an invoke session. */

var last = null;

var a = [1,2,3,4,5,6,7,8];
var b = a.map(function(x) {
    x++;
    var res = last ? last() : 0;
    last = function() { return x; };
    return res;
  });

assertEq("" + b, "0,2,3,4,5,6,7,8");
