// |jit-test| error:can't access lexical declaration
x = [];
x.length;
evaluate("x.length; let x = 1");
