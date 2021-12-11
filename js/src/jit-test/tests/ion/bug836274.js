function dumpArgs6(i) {
  if (i == 90)
    return funapply6.arguments.length;
  return [i];
}
function funapply6() {
  return dumpArgs6.apply({}, arguments);
}
function test6(i) {
  return funapply6(i,1,2,3);
}
test6(89)[0]
test6(0.2)
