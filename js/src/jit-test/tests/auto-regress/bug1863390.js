var left = "left-rope-child";
var right = newString("right-rope-child", {twoByte: true});
var rope = newRope(left, right);
var result = rope.substring(0, left.length + 1);

assertEq(result, "left-rope-childr");
