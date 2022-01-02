// |jit-test| error:character class
Object.defineProperty(RegExp.prototype, Symbol.search, {get: () => { throw "wrong"; }});
"abc".search("[[");
