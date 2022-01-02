let strict = (function() { return this; })() === undefined;

// Allow this to be used as a JSM
var EXPORTED_SYMBOLS = [];

if (!strict) vu = 1;                                    // Unqualified Variable
var vq = 2;                                             // Qualified Variable
let vl = 3;                                             // Lexical
this.gt = 4;                                            // Global This
eval("this.ed = 5");                                    // Direct Eval
(1,eval)("this.ei = 6");                                // Indirect Eval
(new Function("this.fo = 7"))();                        // Dynamic Function Object
if (!strict) (function() { this.fi = 8; })();           // Indirect Function This
function fd_() { this.fd = 9; }; if (!strict) fd_();    // Direct Function Implicit
