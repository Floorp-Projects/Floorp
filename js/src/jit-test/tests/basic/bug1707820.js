gczeal(4);
let a = "x".repeat(100);
for (let i = 0; i < 50; i++) {
  let s = new String(a);
  s.a = 0;
  s.b = 0;
  delete s.a;
  Object.keys(s);
}
