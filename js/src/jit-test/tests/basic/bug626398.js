
function g(n) {
  var s;
  switch (n) {
    case 0:
    s = "c"+n;
    break;

    default:
    s = "d"+n;
    break;
  }
  return s;
}

// Do it twice with different initial values for 'i' to allow for 8
// being even or odd.

var s = "";
for (let i = 0; i != 30; i+=2) {
  s += g(i%4/2);
}
assertEq(s, "c0d1c0d1c0d1c0d1c0d1c0d1c0d1c0");

var s = "";
for (let i = 2; i != 30; i+=2) {
  s += g(i%4/2);
}
assertEq(s, "d1c0d1c0d1c0d1c0d1c0d1c0d1c0");

