function testEmpty() {
  let map = new Map();
  for (let i = 0; i < 100; ++i) {
    assertEq(map.size, 0);
  }
}
for (let i = 0; i < 2; ++i) testEmpty();

function testSimple() {
  let map = new Map([1, 2, 3, 4].entries());
  for (let i = 0; i < 100; ++i) {
    assertEq(map.size, 4);
  }
}
for (let i = 0; i < 2; ++i) testSimple();

function testWithDelete() {
  for (let i = 0; i < 100; ++i) {
    let a = [1, 2, 3, 4];
    let map = new Map(a.entries());
    for (let j = 0; j < a.length; ++j) {
      assertEq(map.size, a.length - j);
      map.delete(j);
      assertEq(map.size, a.length - j - 1);
    }
    assertEq(map.size, 0);
  }
}
for (let i = 0; i < 2; ++i) testWithDelete();

function testWithSet() {
  for (let i = 0; i < 100; ++i) {
    let a = [1, 2, 3, 4];
    let map = new Map();
    for (let j = 0; j < a.length; ++j) {
      assertEq(map.size, j);
      map.set(a[j], 0);
      assertEq(map.size, j + 1);
    }
    assertEq(map.size, a.length);
  }
}
for (let i = 0; i < 2; ++i) testWithSet();
