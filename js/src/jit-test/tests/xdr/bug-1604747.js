let og = parseModule("1");
let bc = codeModule(og);
let m54 = decodeModule(bc);
m54.declarationInstantiation();

try {
    bc = codeModule(m54);
}
catch {}
