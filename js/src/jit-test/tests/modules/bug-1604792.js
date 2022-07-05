var lfLogBuffer = `
  eval("function f(){}; f();");
`;

let lfMod = parseModule(lfLogBuffer);
moduleLink(lfMod);
moduleEvaluate(lfMod);
