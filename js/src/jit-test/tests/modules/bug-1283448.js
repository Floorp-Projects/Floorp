let a = registerModule('a', parseModule("var x = 1; export { x };"));
let b = registerModule('b', parseModule("import { x as y } from 'a';"));
a.__proto__ = {15: 1337};
moduleLink(b);
