function f() {}
var g = new Function();
delete Function;
function h() {}

assertEq(f.__proto__, g.__proto__);
assertEq(g.__proto__, h.__proto__);
assertEq(false, "Function" in this);

reportCompare("ok", "ok", "bug 569306");
