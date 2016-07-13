// |jit-test| error: InternalError

// This tests that attempting to perform ModuleDeclarationInstantation a second
// time after a failure throws an error. Doing this would be a bug in the module
// loader, which is expected to throw away modules if there is an error
// instantiating them.
//
// The first attempt fails becuase module 'a' is not available. The second
// attempt fails because of the previous failure (it would otherwise succeed as
// 'a' is now available).

let moduleRepo = {};
setModuleResolveHook(function(module, specifier) {
    return moduleRepo[specifier];
});
try {
    let b = moduleRepo['b'] = parseModule("export var b = 3; export var c = 4;");
    let c = moduleRepo['c'] = parseModule("export * from 'a'; export * from 'b';");
    c.declarationInstantiation();
} catch (exc) {}
let a = moduleRepo['a'] = parseModule("export var a = 1; export var b = 2;");
let d = moduleRepo['d'] = parseModule("import { a } from 'c'; a;");
d.declarationInstantiation();
