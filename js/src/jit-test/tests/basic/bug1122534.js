// |jit-test| error: TypeError

function newFunc(x) { new Function(x)(); };
newFunc("\
function GetUnicodeValues(c) {\
    u = new Array();\
    if ((c >= 0x0100 && c < 0x0138) || (c > 0x1Bedb  && c < 0x0178))\
        return;\
    return u;\
}\
function Unicode(c) {\
    u = GetUnicodeValues(c);\
    this.upper = u[0];\
}\
for (var i = 0; i <= 0x017f; i++) { var U = new Unicode(i); }\
");
