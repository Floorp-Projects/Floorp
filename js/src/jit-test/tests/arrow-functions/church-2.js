// Church-Peano integers

var Zero = f => x => x;
var Succ = n => f => x => n(f)(f(x));
var Add = a => b => f => x => a(f)(b(f)(x));
var Mul = a => b => f => x => a(b(f))(x);
var Exp = a => b => b(a);

var n = f => f(k => k + 1)(0);

assertEq(n(Zero), 0);
assertEq(n(Succ(Zero)), 1);
assertEq(n(Succ(Succ(Zero))), 2);

var Three = Succ(Succ(Succ(Zero)));
var Five = Succ(Succ(Three));
assertEq(n(Add(Three)(Five)), 8);
assertEq(n(Mul(Three)(Five)), 15);
assertEq(n(Exp(Three)(Five)), 243);
