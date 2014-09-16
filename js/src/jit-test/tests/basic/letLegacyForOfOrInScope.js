var x = "foobar";
{ for (let x of x) assertEq(x.length, 1, "second x refers to outer x"); }

var x = "foobar";
{ for (let x in x) assertEq(x.length, 1, "second x refers to outer x"); }
