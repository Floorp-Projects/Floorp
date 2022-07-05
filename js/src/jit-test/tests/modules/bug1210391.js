let a = registerModule('a', parseModule("export var a = 1; export var b = 2;"));
let b = registerModule('b', parseModule("export var b = 3; export var c = 4;"));
let c = registerModule('c', parseModule("export * from 'a'; export * from 'b';"));
let d = registerModule('d', parseModule("import { a } from 'c'; a;"));
moduleLink(d);
moduleEvaluate(d);

