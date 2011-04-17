function foo1(x, n) {
  var i = 0;
  while (--n >= 0) {
    x[i++] = 0;
  }
}
foo1([1,2,3,4,5],5);

function foo2(x, n) {
  var i = 0;
  while (--n >= 0) {
    x[i++] = 0;
  }
}
foo2([1,2,3,4,5],6);

function foo3(x, n) {
  var i = 0;
  while (n-- >= 0) {
    x[i++] = 0;
  }
}
foo3([1,2,3,4,5],5);

function foo4(x, n) {
  var i = 0;
  while (--n >= 0) {
    x[++i] = 0;
  }
}
foo4([1,2,3,4,5],5);

function foo5(x, n) {
  var i = 0;
  while (--n >= 0) {
    x[++i] = 0;
  }
}
foo5([1,2,3,4,5,6],5);
