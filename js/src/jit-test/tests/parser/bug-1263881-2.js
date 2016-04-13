// |jit-test| error: SyntaxError
let s = "";
let max = 65536;
for (let i = 0; i < max; i++)
  s += "let ns" + i + " = "+ i +";\n";
eval(s);
