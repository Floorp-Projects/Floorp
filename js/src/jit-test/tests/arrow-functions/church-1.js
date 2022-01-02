// Church booleans

var True = t => f => t;
var False = t => f => f;
var bool_to_str = b => b("True")("False");
var And = a => b => a(b)(a);
var Or = a => b => a(a)(b);

assertEq(And(True)(True), True);
assertEq(And(True)(False), False);
assertEq(And(False)(True), False);
assertEq(And(False)(False), False);

assertEq(Or(True)(True), True);
assertEq(Or(True)(False), True);
assertEq(Or(False)(True), True);
assertEq(Or(False)(False), False);
