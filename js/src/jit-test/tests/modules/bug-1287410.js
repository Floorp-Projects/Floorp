// |jit-test| error: InternalError

let moduleRepo = {};
setModuleResolveHook(function(module, specifier) {
    if (specifier in moduleRepo)
        return moduleRepo[specifier];
    throw "Module '" + specifier + "' not found";
});
let a = moduleRepo['a'] = parseModule("export var a = 1; export var b = 2;");
let b = moduleRepo['b'] = parseModule("export var b = 3; export var c = 4;");
let c = moduleRepo['c'] = parseModule("export * from 'a'; export * from 'b';");
c.declarationInstantiation();

// Module 'a' is replaced with another module that has not been instantiated.
// This should not happen and would be a bug in the module loader.
a = moduleRepo['a'] = parseModule("export var a = 1; export var b = 2;");

let d = moduleRepo['d'] = parseModule("import { a } from 'c'; a;");

// Attempting to instantiate 'd' throws an error because depdency 'a' of
// instantiated module 'c' is not instantiated.
d.declarationInstantiation();
