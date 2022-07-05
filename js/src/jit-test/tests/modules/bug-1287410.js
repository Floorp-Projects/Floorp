// |jit-test| error: InternalError

let a = registerModule('a', parseModule("export var a = 1; export var b = 2;"));
let b = registerModule('b', parseModule("export var b = 3; export var c = 4;"));
let c = registerModule('c', parseModule("export * from 'a'; export * from 'b';"));
moduleLink(c);

// Module 'a' is replaced with another module that has not been instantiated.
// This should not happen and would be a bug in the module loader.
a = registerModule('a', parseModule("export var a = 1; export var b = 2;"));

let d = registerModule('d', parseModule("import { a } from 'c'; a;"));

// Attempting to instantiate 'd' throws an error because depdency 'a' of
// instantiated module 'c' is not instantiated.
moduleLink(d);
moduleEvaluate(d);
