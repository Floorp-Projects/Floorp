function getArgs() { return arguments; }
var a1 = getArgs(1);
var a2 = getArgs(1);
a1.__proto__ = a2;
delete a1[0];
a1[0];
