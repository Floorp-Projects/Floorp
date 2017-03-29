// |jit-test| error: InternalError

// This tests that attempting to perform ModuleDeclarationInstantation a second
// time after a failure throws an error. Doing this would be a bug in the module
// loader, which is expected to throw away modules if there is an error
// instantiating them.
//
// The first attempt fails becuase module 'a' is not available. The second
// attempt fails because of the previous failure.
//
// This test exercises the path where the previously instantiated module is
// re-instantiated directly.

let moduleRepo = {};
setModuleResolveHook(function(module, specifier) {
    return moduleRepo[specifier];
});
let c;
try {
    let b = moduleRepo['b'] = parseModule("export var b = 3; export var c = 4;");
    c = moduleRepo['c'] = parseModule("export * from 'a'; export * from 'b';");
    c.declarationInstantiation();
} catch (exc) {}
c.declarationInstantiation();
