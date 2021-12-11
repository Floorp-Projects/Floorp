foo(1, '2', bar())

foo()
  .bar()
  .bazz()

console.log('yo')

foo(
  1,
  bar()
)

var a = 3;

// set a step point at the first call expression in step expressions
var x = { a: a(), b: b(), c: c() };
var b = [ foo() ];
[ a(), b(), c() ];
(1, a(), b());
x(1, a(), b());
