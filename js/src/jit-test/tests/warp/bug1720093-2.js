function throws() { with ({}) {} throw 1; }

function main() {
  let obj = {2294967295: 0, 2294967296: 1};
  let x;

  for (let i = 0; i < 110; i++) {
    let c = 2294967296;
    x = --c;
    Math.fround(0);
  }

  try {
    return throws()
  } catch {
    assertEq(obj[x], 0);
  }
}
main();
