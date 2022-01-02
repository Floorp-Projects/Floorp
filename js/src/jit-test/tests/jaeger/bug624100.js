// |jit-test| error: ReferenceError
eval("'use strict'; for(let j=0;j<9;++j) {} x;");
