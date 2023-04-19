a = `
  fullcompartmentchecks(true);
  oomTest(Debugger)
`.split('\n')
c = "";
while (true) {
    d = a.shift()
    if (d == null) break;
    c += d
    e(c);
    function e(f) {
        try {
            evaluate(f);
        } catch {}
    }
}
