var count = 0;
var scope = {
  get args() {
    count++;
    return "";
  }
};

with (scope) {
  [].push(...args);
}

// Ensure |args| is only looked up once.
assertEq(count, 1);
