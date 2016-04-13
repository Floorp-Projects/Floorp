// |jit-test| error: SyntaxError
let s = "function foo() {\n";
let max = 65536;
for (let i = 0; i < max; i++)
  s += "let ns" + i + " = "+ i +";\n";
s += "};";
eval(s);
