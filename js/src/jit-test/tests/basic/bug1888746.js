function comparator(x, y) {
  saveStack();
  return {valueOf: function() {
    saveStack();
    return x - y;
  }};
}
for (let i = 0; i < 20; i++) {
  let arr = [3, 1, 2];
  arr.sort(comparator);
  assertEq(arr.toString(), "1,2,3");
}
