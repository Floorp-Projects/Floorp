let moduleRepo = {};
setModuleResolveHook(function(module, specifier) {
        return moduleRepo[specifier];
});
moduleRepo['a'] = parseModule("export let a = 1;");
let s = "";
let max = 65536;
for (let i = 0; i < max; i++)
  s += "import * as ns" + i + " from 'a';\n";
parseModule(s);
