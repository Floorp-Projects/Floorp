// |jit-test| error: SyntaxError
"use strict";
function inner() (([arguments, b] = this, c)());
