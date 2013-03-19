// Arrow right-associativity with +=

var s = "";
s += x => x.name;
assertEq(s, "x => x.name");
