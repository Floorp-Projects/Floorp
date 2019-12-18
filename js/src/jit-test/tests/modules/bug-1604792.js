var lfLogBuffer = `
  eval("function f(){}; f();");
`;

let lfMod = parseModule(lfLogBuffer);
lfMod.declarationInstantiation();
lfMod.evaluation();
