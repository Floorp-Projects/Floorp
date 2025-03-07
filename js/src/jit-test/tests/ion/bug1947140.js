// |jit-test| --ion-inlining=off

function foo(x) { return "a sufficiently long string"; }
function opt(){
  return "E" >= "E" + foo();
}

for (let i = 0; i < 55; i++) {
  assertEq(opt(), false);
}
