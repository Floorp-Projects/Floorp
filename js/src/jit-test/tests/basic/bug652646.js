function foo() {
  try {
    return 0;
  } catch (e) {
    try {
      return 1;
    } catch (e) {
    }
  }
}
foo();
