var rg = /(X(?:.(?!X))*?Y)|(Y(?:.(?!Y))*?Z)/g;
var str = "Y aaa X Match1 Y aaa Y Match2 Z";
assertEq(str.match(rg) + "", "X Match1 Y,Y Match2 Z");
