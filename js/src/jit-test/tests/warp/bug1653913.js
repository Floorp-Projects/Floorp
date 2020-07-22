var s = "aaaaaaaaaaaa";
var a = [, [...s]];
assertEq(a.toString(), ",a,a,a,a,a,a,a,a,a,a,a,a");
