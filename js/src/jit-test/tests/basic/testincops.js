var globalinc = 0;
function testincops(n) {
  var i = 0, o = {p:0}, a = [0];
  n = 100;

  for (i = 0; i < n; i++);
  while (i-- > 0);
  for (i = 0; i < n; ++i);
  while (--i >= 0);

  for (o.p = 0; o.p < n; o.p++) globalinc++;
  while (o.p-- > 0) --globalinc;
  for (o.p = 0; o.p < n; ++o.p) ++globalinc;
  while (--o.p >= 0) globalinc--;

  ++i; // set to 0
  for (a[i] = 0; a[i] < n; a[i]++);
  while (a[i]-- > 0);
  for (a[i] = 0; a[i] < n; ++a[i]);
  while (--a[i] >= 0);

  return [++o.p, ++a[i], globalinc].toString();
}
assertEq(testincops(), "0,0,0");
