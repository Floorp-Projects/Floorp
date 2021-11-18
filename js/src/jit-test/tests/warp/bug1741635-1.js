function foo() {}

var i;
function c(x, ...rest) {
  for (i = 0; i < rest.length; i++)
    foo(rest[i])
}

for (var j = 0; j < 100; j++) {
  c(0, 0);
}
