// 'let' after "use strict" directive without semicolon is lexed as TOK_NAME
// before parsing the directive.  'let' with TOK_NAME should be handled
// correctly in strict mode.

"use strict"
let a = 1;
