// |jit-test| error: InternalError

let m = parseModule(`
  let c = parseModule(\`
    import "a";
  \`);
  c.declarationInstantiation();
`);
setModuleResolveHook(function(module, specifier) { return m; });
m.declarationInstantiation();
m.evaluation();
