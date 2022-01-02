var res = "class { constructor() {} }";
var test = eval("(" + res + ").toString()");

assertEq(test, res);