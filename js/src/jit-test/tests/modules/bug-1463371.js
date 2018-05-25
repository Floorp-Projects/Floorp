// |jit-test| error: Error

var g = newGlobal();
g.eval(`
  setModuleResolveHook(function(module, specifier) { return module; });
`);
let m = parseModule(`
  import {} from './foo.js';
`);
m.declarationInstantiation();
