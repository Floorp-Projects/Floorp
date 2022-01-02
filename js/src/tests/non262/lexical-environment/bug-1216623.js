// Scoping in the head of for(let;;) statements.

let x = 0;
for (let i = 0, a = () => i; i < 4; i++) {
  assertEq(i, x++);
  assertEq(a(), 0);
}
assertEq(x, 4);

x = 11;
let q = 0;
for (let {[++q]: r} = [0, 11, 22], s = () => r; r < 13; r++) {
  assertEq(r, x++);
  assertEq(s(), 11);
}
assertEq(x, 13);
assertEq(q, 1);

reportCompare(0, 0);
