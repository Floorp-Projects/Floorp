// |jit-test| need-for-each

z = [];
m = evalcx("");
Object.freeze(m);
for each(l in [{}, {}]) {
  m.s = "";
}
