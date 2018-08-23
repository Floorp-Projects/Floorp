// |jit-test| error: InternalError

let m = parseModule(`
  let c = parseModule(\`
    import "a";
  \`);
  instantiateModule(c);
`);
setModuleResolveHook(function(module, specifier) { return m; });
instantiateModule(m);
evaluateModule(m);
