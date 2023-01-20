var caught = false;
function* a() {
  try {
    try {
      yield;
    } finally {
      for (b = 0; b < 20; b++) {}
    }
  } catch (e) { caught = true; }
}
c = a();
c.next();
c.return();
assertEq(caught, false);
