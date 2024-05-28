function inner(cond) {
  var x;
  if (cond) {
    x = true;
  } else {
    x = false;
   switch (this) {}
  }
  return x;
}

function outer(cond) {
  if (inner(cond)) {
    return 1;
  } else {
    return 2;
  }
}

with ({}) {}
for (var i = 0; i < 1000; i++) {
  outer(true);
  outer(false);
}
