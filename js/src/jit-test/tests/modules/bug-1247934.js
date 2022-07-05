setJitCompilerOption("ion.warmup.trigger", 50);
s = "";
for (i = 0; i < 1024; i++) s += "export let e" + i + "\n";
registerModule('a', parseModule(s));
moduleLink(parseModule("import * as ns from 'a'"));
