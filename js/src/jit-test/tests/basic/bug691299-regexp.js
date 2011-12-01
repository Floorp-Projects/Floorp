// |jit-test| error: SyntaxError: invalid quantifier

String.fromCharCode(256).replace(/[^a$]{4294967295}/,"aaaa");
