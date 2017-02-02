var g = newGlobal();
var b = g.eval(`
var b = /foo2/;
Object.defineProperty(b, "source", { get: () => {}});
`);
new RegExp(b).source;
