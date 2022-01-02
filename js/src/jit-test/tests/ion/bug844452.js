function reportCompare (expected, actual) {
  return expected != actual;
}

function wrap() {
  reportCompare(true, true);
}

reportCompare('', '');
wrap();
