// |jit-test| error: TypeError
var s = "12345";
for(var i=0; i<7; i++) {
    print(s[i].length);
}

