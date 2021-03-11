gczeal(0);
try { evaluate(`
  for (let v3 = 0; v3 < 256; v3++)
    x1 = this.__defineSetter__("x", function(z69) { })
  const v9 = {};
  const v10 = v9[gczeal(4)];
`); } catch(exc) {}
for (let v3 = 0; v3 < 256; v3++)
  x1 = this.__defineSetter__("x", function(z69) {})
