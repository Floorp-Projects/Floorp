function getprop (obj) {
  return obj.nonexist;
}

for (var n = 0; n < 100; n++) {
  var a = (n % 2) ? ((n % 3) ? new Object() : new Object()) : new Object();
  getprop(a);
}
